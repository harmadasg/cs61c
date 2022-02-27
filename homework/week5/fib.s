# A)

fib:
		addiu $sp $sp, -12
		sw $ra, 0($sp)
		sw $s0, 4($sp)
		sw $s1, 8($sp)
		
		move $s0, $a0
		beq $s0, $0, return_0
		addiu $t0, $0, 1
		beq $s0, $t0, return_1
		
		addi $a0, $s0, -1
		jal fib
		move $s1, $v0
		addi $a0, $s0, -2
		jal fib
		addu $v0, $s1, $v0
		j epilogue
		
return_1:
		addiu $v0, $0, 1
		j epilogue
return_0:
		move $v0, $0
epilogue:
		lw $s1, 8($sp)
		lw $s0, 4($sp)
		lw $ra, 0($sp)
		addiu $sp $sp, 12
		jr $ra

# B)

fib:
		addiu $sp $sp, -12
		sw $ra, 0($sp)
		sw $a0, 4($sp)
		sw $s0, 8($sp)
		
		beq $a0, $0, return_0
		addiu $t0, $0, 1
		beq $a0, $t0, return_1
		
		sll $t0, $a0, 2
		addu $t0, $a1, $t0
		lw $v0, 0($t0)
		bne $v0, $0 epilogue
		
		addi $a0, $a0, -1
		jal fib
		move $s0, $v0
		lw $a0, 4($sp)
		addi $a0, $a0, -2
		jal fib
		addu $v0, $s0, $v0
		
		lw $a0, 4($sp)
		sll $t0, $a0, 2
		addu $t0, $a1, $t0
		sw $v0, 0($t0)
		j epilogue
		
return_1:
		addiu $v0, $0, 1
		j epilogue
return_0:
		move $v0, $0
epilogue:
		lw $s0, 8($sp)
		lw $a0, 4($sp)
		lw $ra, 0($sp)
		addiu $sp $sp, 12
		jr $ra
