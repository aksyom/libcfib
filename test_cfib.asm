	.text
	.intel_syntax noprefix
	.file	"test_cfib.c"
	.globl	test_fiber
	.p2align	4, 0x90
	.type	test_fiber,@function
test_fiber:                             # @test_fiber
# BB#0:
	push	rbp
	mov	rbp, rsp
	push	r15
	push	r14
	push	rbx
	push	rax
	mov	edi, .Lstr
	call	puts
	mov	rcx, qword ptr [rip + main_context]
	mov	r14, qword ptr [rip + cfib_current@GOTTPOFF]
	mov	rbx, qword ptr fs:[0]
	mov	r15, qword ptr [rbx + r14]
	mov	qword ptr [rbx + r14], rcx
	mov	rdi, qword ptr [rip + __stderrp]
	mov	esi, .L.str.4
	xor	eax, eax
	mov	rdx, r15
	call	fprintf
	mov	rdi, qword ptr [rip + __stderrp]
	call	fflush
	mov	rax, qword ptr [rbx + r14]
	mov	rsi, qword ptr [rax]
	mov	rdi, r15
	call	_cfib_swap
	mov	edi, .Lstr.5
	add	rsp, 8
	pop	rbx
	pop	r14
	pop	r15
	pop	rbp
	jmp	puts                    # TAILCALL
.Lfunc_end0:
	.size	test_fiber, .Lfunc_end0-test_fiber

	.globl	main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
# BB#0:
	push	rbp
	mov	rbp, rsp
	push	r15
	push	r14
	push	r12
	push	rbx
	xor	eax, eax
	call	cfib_init_thread
	mov	qword ptr [rip + main_context], rax
	mov	edi, 24
	call	malloc
	mov	r12, rax
	mov	esi, test_fiber
	xor	edx, edx
	xor	ecx, ecx
	mov	rdi, r12
	call	cfib_init
	mov	edi, .Lstr.6
	call	puts
	mov	r15, qword ptr [rip + cfib_current@GOTTPOFF]
	mov	rbx, qword ptr fs:[0]
	mov	r14, qword ptr [rbx + r15]
	mov	qword ptr [rbx + r15], r12
	mov	rdi, qword ptr [rip + __stderrp]
	mov	esi, .L.str.4
	xor	eax, eax
	mov	rdx, r14
	mov	rcx, r12
	call	fprintf
	mov	rdi, qword ptr [rip + __stderrp]
	call	fflush
	mov	rax, qword ptr [rbx + r15]
	mov	rsi, qword ptr [rax]
	mov	rdi, r14
	call	_cfib_swap
	mov	edi, .Lstr.7
	call	puts
	mov	r14, qword ptr [rbx + r15]
	mov	qword ptr [rbx + r15], r12
	mov	rdi, qword ptr [rip + __stderrp]
	mov	esi, .L.str.4
	xor	eax, eax
	mov	rdx, r14
	mov	rcx, r12
	call	fprintf
	mov	rdi, qword ptr [rip + __stderrp]
	call	fflush
	mov	rax, qword ptr [rbx + r15]
	mov	rsi, qword ptr [rax]
	mov	rdi, r14
	call	_cfib_swap
	xor	eax, eax
	pop	rbx
	pop	r12
	pop	r14
	pop	r15
	pop	rbp
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main

	.type	main_context,@object    # @main_context
	.comm	main_context,8,8
	.type	.L.str.4,@object        # @.str.4
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str.4:
	.asciz	"%p -> %p\n"
	.size	.L.str.4, 10

	.type	.Lstr,@object           # @str
.Lstr:
	.asciz	"test_fiber 1"
	.size	.Lstr, 13

	.type	.Lstr.5,@object         # @str.5
.Lstr.5:
	.asciz	"test_fiber 2"
	.size	.Lstr.5, 13

	.type	.Lstr.6,@object         # @str.6
.Lstr.6:
	.asciz	"main 1"
	.size	.Lstr.6, 7

	.type	.Lstr.7,@object         # @str.7
.Lstr.7:
	.asciz	"main 2"
	.size	.Lstr.7, 7


	.ident	"FreeBSD clang version 4.0.0 (tags/RELEASE_400/final 297347) (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",@progbits
