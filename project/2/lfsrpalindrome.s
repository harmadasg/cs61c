main:
	lui $ra, 0x10
	lui $sp, 0x2000
	lui $v0, 0x0000
	addiu $a0, $0, 1
	jal LfsrPalindrome
halt:
	beq $0, $0, halt

# seed  is in $a0
LfsrPalindrome:
	addiu	$sp, $sp, -4
	sw	$ra, 0($sp)
	addiu $t0, $a0, 0
	
loop:
	bitpal $t1, $t0
	bne $0, $t1, return
	lfsr $t0, $t0
	beq $t0, $a0, return
	j loop
	
return:	
	addiu $v0, $t0, 0
	lw	$ra, 0($sp)
	addiu	$sp, $sp, 4
	jr $ra
