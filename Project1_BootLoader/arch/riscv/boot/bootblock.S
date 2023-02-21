#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a0, BIOS_PUTSTR
	la a1, msg
	call bios_func_entry


	// TODO: [p1-task2] call BIOS to read kernel in SD card
/*	li a0, BIOS_SDREAD
	li a1, kernel
	li a2, 15
	li a3, 1
	call bios_func_entry
*/
    la s0, os_size_loc
    lh s0, 0(s0)
    li s1, 1
    li s2, 512
    la s3, kernel
do_read_sector:
	li a0, BIOS_SDREAD
    mv a1, s3
    li a2, 1
    mv a3, s1
    call bios_func_entry
    addi s1, s1, 1
    add  s3, s3, s2
    addi s0, s0, -1
    bgtz s0, do_read_sector

	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	la x1, kernel
	jr x1

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

