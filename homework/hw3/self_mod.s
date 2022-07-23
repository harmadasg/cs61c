nextshort:
		addiu $v0, $0, 0
		la $t0, nextshort
		lw $t1, 0($t0)
		addiu $t3, $0, 0xFFFF
		and $t2, $t1, $t3
		beq $t2 , $t3 , HandleSpecial
		addiu $t1, $t1, 1
		sw $t1, 0($t0)
		jr $ra
HandleSpecial:
		la $t6, backupinst
		lw $t1, 0($t6)
		sw $t1, 0($t0)
		jr $ra
backupinst:
		addiu $v0, $0, 0
