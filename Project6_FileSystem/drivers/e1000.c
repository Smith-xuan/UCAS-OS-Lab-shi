#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

volatile uint64_t plic_addr;
volatile uint32_t nr_irqs;

// E1000 Tx & Rx Descriptors
struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    int i = 0;
    /* TODO: [p5-task1] Initialize tx descriptors */
    memset(tx_desc_array, 0, sizeof(tx_desc_array));
    for (i = 0; i < TXDESCS; i++){
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
    }
    
    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)kva2pa(tx_desc_array)); //?
    e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array)); 

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    int i = 0;
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    e1000_write_reg_array(e1000, E1000_RA, 0, 0x00350a00);
    e1000_write_reg_array(e1000, E1000_RA, 1, 0x8000531e);

    /* TODO: [p5-task2] Initialize rx descriptors */
    memset(rx_desc_array, 0, sizeof(rx_desc_array));
    for (i = 0; i < RXDESCS; i++){
        rx_desc_array[i].addr = kva2pa(rx_pkt_buffer[i]);
    }
    
    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(kva2pa(rx_desc_array)));
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS-1);

    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    uint32_t tail;
    struct e1000_tx_desc *desc;

    tail = e1000_read_reg(e1000, E1000_TDT);
    desc = &tx_desc_array[tail];

    //all descriptors are used
    if((desc->status & E1000_TXD_STAT_DD) == 0){
        return 0;
    }

    //clear the buffer and fill the pakt
    memset(tx_pkt_buffer[tail], 0, TX_PKT_SIZE*sizeof(char));
    memcpy(tx_pkt_buffer[tail],(uint8_t*)txpacket, length);

    //fill the descrtptor
    desc->addr = (kva2pa)(tx_pkt_buffer[tail]);
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
    desc->length = length;

    //modify the TDT
    e1000_write_reg(e1000, E1000_TDT, (tail+1)%TXDESCS);

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    uint32_t tail;
    struct e1000_rx_desc *desc;

    tail = (e1000_read_reg(e1000, E1000_RDT)+1) % RXDESCS;
    desc = &rx_desc_array[tail];

    //all descriptors are used
    if((desc->status & E1000_RXD_STAT_DD) == 0){
        return 0;
    }

    //copy the buffer to dest
    memcpy((uint8_t*)rxbuffer, rx_pkt_buffer[tail], desc->length);

    //clear the rx_descriptor
    desc->status = 0;

    //modify the TDT
    e1000_write_reg(e1000, E1000_RDT, (tail)%RXDESCS);


    return desc->length;
}