	.file	1 "lab3_ex3.c"
	.section .mdebug.eabi32
	.previous
	.section .gcc_compiled_long32
	.previous
	.gnu_attribute 4, 1
	.text
	.align	2
	.globl	main
	.set	nomips16
	.ent	main
	.type	main, @function
main:
	.frame	$sp,0,$31		# vars= 0, regs= 0/0, args= 0, gp= 0
	.mask	0x00000000,0
	.fmask	0x00000000,0
	lui	$6,%hi(source)		# pointer for source
	lw	$3,%lo(source)($6)	# dereference the pointer
	beq	$3,$0,$L2			# check if the value is equal to zero
	lui	$7,%hi(dest)		# pointer for dest
	addiu	$7,$7,%lo(dest)
	addiu	$6,$6,%lo(source)
	move	$2,$0			# index variable k initialized to 0
$L3:						# start of the loop body
	addu	$5,$7,$2		# add index to dest pointer
	addu	$4,$6,$2		# add index to source pointer
	sw	$3,0($5)			# copy value from source to dest
	lw	$3,4($4)			# load the next value from the source array
	addiu	$2,$2,4			# increase the index by 1 word (4 byte)
	bne	$3,$0,$L3			# check the loop condition
$L2:								
	move	$2,$0
	j	$31
	.end	main
	.size	main, .-main
	.globl	source
	.data
	.align	2
	.type	source, @object
	.size	source, 28
source:						# values for source are stored here
	.word	3
	.word	1
	.word	4
	.word	1
	.word	5
	.word	9
	.word	0

	.comm	dest,40,4		# dest is stored here
	.ident	"GCC: (GNU) 4.4.1"
