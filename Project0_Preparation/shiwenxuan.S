.global main
main:
	li s0, 0//i=0
	li s1, 0x64//100
	li s2, 0x1
	li s3, 0x0

	bge s0, s1, end
loop:
	add s3, s3, s2
	addi s2, s2, 1
	addi s0, s0, 1 
	blt s0, s1, loop
// TODO: [p0-task1] Add your code here!
end:
