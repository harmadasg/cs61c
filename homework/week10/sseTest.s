	.file	"sseTest.c"
	.intel_syntax noprefix
	.text
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC3:
	.string	"|%g %g| * |%g %g| = |%g %g|\n"
.LC8:
	.string	"|%g %g|   |%g %g|   |%g %g|\n"
	.section	.text.startup,"ax",@progbits
	.p2align 4
	.globl	main
	.type	main, @function
main:
.LFB5699:
	.cfi_startproc
	endbr64
	sub	rsp, 8
	.cfi_def_cfa_offset 16
	pxor	xmm3, xmm3
	lea	rsi, .LC3[rip]
	movsd	xmm1, QWORD PTR .LC1[rip]
	movsd	xmm6, QWORD PTR .LC0[rip]
	mov	edi, 1
	mov	eax, 6
	movsd	xmm0, QWORD PTR .LC2[rip]
	movapd	xmm4, xmm1
	movapd	xmm2, xmm1
	movapd	xmm5, xmm6
	call	__printf_chk@PLT
	mov	rax, QWORD PTR .LC0[rip]
	movsd	xmm0, QWORD PTR .LC6[rip]
	pxor	xmm2, xmm2
	movsd	xmm5, QWORD PTR .LC5[rip]
	movsd	xmm1, QWORD PTR .LC7[rip]
	mov	edi, 1
	lea	rsi, .LC8[rip]
	movq	xmm4, rax
	movapd	xmm3, xmm0
	mov	eax, 6
	call	__printf_chk@PLT
	xor	eax, eax
	add	rsp, 8
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE5699:
	.size	main, .-main
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	0
	.long	1075314688
	.align 8
.LC1:
	.long	0
	.long	1074266112
	.align 8
.LC2:
	.long	0
	.long	1072693248
	.align 8
.LC5:
	.long	0
	.long	1075838976
	.align 8
.LC6:
	.long	0
	.long	1073741824
	.align 8
.LC7:
	.long	0
	.long	1074790400
	.ident	"GCC: (Ubuntu 11.2.0-19ubuntu1) 11.2.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
