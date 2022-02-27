# A)
		addiu $sp $sp, -8
		sw $ra, 0($sp)
		sw $s0, 4($sp)
		
		move $s0, $a0 
		lbu $a0, 0($s0)
while:	
		beq $a0, $0, return_a
		jal print
		addiu $s0, $s0, 1
		lbu $a0, 0($s0)
		j while
return_a:	
		lw $s0, 4($sp)
		lw $ra, 0($sp)
		addiu $sp $sp, 8
		jr $ra

# B)
		addiu $sp $sp, -16
		sw $ra, 0($sp)
		sw $s0, 4($sp)
		sw $s1, 8($sp)
		sw $s2, 12($sp)
		
		move $s1, $0
		move $s2, $a1
		move $s0, $a0
		
for_loop:
		beq $s2 , $s1, return_b
		addu $a0, $s1, $s0
		lbu $a0, 0($a0)
		jal print
		addiu $s1, $s1, 1
		j for_loop
		
return_b:
		lw $s2, 12($sp)
		lw $s1, 8($sp)
		lw $s0, 4($sp)
		lw $ra, 0($sp)
		addiu $sp $sp, 16
		jr $ra

# C)
		addiu $sp $sp, -8
		sw $ra, 0($sp)
		sw $s0, 4($sp)
		
		move $s0, $a0
		lbu $a0, 0($s0)
		
		beq $a0, $0, return_c
do:		
		jal print
		addiu $s0, $s0, 1
		lbu $a0, 0($s0)
		bne $a0, $0, do

return_c:	
		lw $s0, 4($sp)
		lw $ra, 0($sp)
		addiu $sp $sp, 8
		jr $ra
