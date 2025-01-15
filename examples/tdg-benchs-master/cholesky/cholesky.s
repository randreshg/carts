	.text
	.file	"cholesky.c"
	.globl	add_to_diag_hierarchical        # -- Begin function add_to_diag_hierarchical
	.p2align	4, 0x90
	.type	add_to_diag_hierarchical,@function
add_to_diag_hierarchical:               # @add_to_diag_hierarchical
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movl	%edx, -16(%rbp)
	movss	%xmm0, -20(%rbp)
	movl	$0, -24(%rbp)
.LBB0_1:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-24(%rbp), %eax
	movl	-16(%rbp), %ecx
	imull	-12(%rbp), %ecx
	cmpl	%ecx, %eax
	jge	.LBB0_4
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB0_1 Depth=1
	movss	-20(%rbp), %xmm0                # xmm0 = mem[0],zero,zero,zero
	cvtss2sd	%xmm0, %xmm0
	movq	-8(%rbp), %rax
	movq	%rax, -40(%rbp)                 # 8-byte Spill
	movl	-24(%rbp), %eax
	cltd
	idivl	-12(%rbp)
	movl	%eax, %ecx
	imull	-16(%rbp), %ecx
	movl	-24(%rbp), %eax
	cltd
	idivl	-12(%rbp)
	movl	%eax, %edx
	movq	-40(%rbp), %rax                 # 8-byte Reload
	addl	%edx, %ecx
	movslq	%ecx, %rcx
	movq	(%rax,%rcx,8), %rax
	movq	%rax, -32(%rbp)                 # 8-byte Spill
	movl	-24(%rbp), %eax
	cltd
	idivl	-12(%rbp)
	movl	%edx, %ecx
	imull	-12(%rbp), %ecx
	movl	-24(%rbp), %eax
	cltd
	idivl	-12(%rbp)
	movq	-32(%rbp), %rax                 # 8-byte Reload
	addl	%edx, %ecx
	movslq	%ecx, %rcx
	addsd	(%rax,%rcx,8), %xmm0
	movsd	%xmm0, (%rax,%rcx,8)
# %bb.3:                                # %for.inc
                                        #   in Loop: Header=BB0_1 Depth=1
	movl	-24(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -24(%rbp)
	jmp	.LBB0_1
.LBB0_4:                                # %for.end
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	add_to_diag_hierarchical, .Lfunc_end0-add_to_diag_hierarchical
	.cfi_endproc
                                        # -- End function
	.globl	add_to_diag                     # -- Begin function add_to_diag
	.p2align	4, 0x90
	.type	add_to_diag,@function
add_to_diag:                            # @add_to_diag
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movsd	%xmm0, -24(%rbp)
	movl	$0, -28(%rbp)
.LBB1_1:                                # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-28(%rbp), %eax
	cmpl	-12(%rbp), %eax
	jge	.LBB1_4
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB1_1 Depth=1
	movsd	-24(%rbp), %xmm0                # xmm0 = mem[0],zero
	movq	-8(%rbp), %rax
	movl	-28(%rbp), %ecx
	movl	-28(%rbp), %edx
	imull	-12(%rbp), %edx
	addl	%edx, %ecx
	movslq	%ecx, %rcx
	addsd	(%rax,%rcx,8), %xmm0
	movsd	%xmm0, (%rax,%rcx,8)
# %bb.3:                                # %for.inc
                                        #   in Loop: Header=BB1_1 Depth=1
	movl	-28(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -28(%rbp)
	jmp	.LBB1_1
.LBB1_4:                                # %for.end
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	add_to_diag, .Lfunc_end1-add_to_diag
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function get_time
.LCPI2_0:
	.quad	0x3eb0c6f7a0b5ed8d              # double 9.9999999999999995E-7
	.text
	.globl	get_time
	.p2align	4, 0x90
	.type	get_time,@function
get_time:                               # @get_time
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	leaq	-16(%rbp), %rdi
	xorl	%eax, %eax
	movl	%eax, %esi
	callq	gettimeofday@PLT
	movsd	get_time.gtod_ref_time_sec(%rip), %xmm0 # xmm0 = mem[0],zero
	xorps	%xmm1, %xmm1
	ucomisd	%xmm1, %xmm0
	jne	.LBB2_2
	jp	.LBB2_2
# %bb.1:                                # %if.then
	cvtsi2sdq	-16(%rbp), %xmm0
	movsd	%xmm0, get_time.gtod_ref_time_sec(%rip)
.LBB2_2:                                # %if.end
	cvtsi2sdq	-16(%rbp), %xmm0
	subsd	get_time.gtod_ref_time_sec(%rip), %xmm0
	movsd	%xmm0, -24(%rbp)
	movsd	-24(%rbp), %xmm1                # xmm1 = mem[0],zero
	cvtsi2sdq	-8(%rbp), %xmm0
	movsd	.LCPI2_0(%rip), %xmm2           # xmm2 = mem[0],zero
	mulsd	%xmm2, %xmm0
	addsd	%xmm1, %xmm0
	movsd	%xmm0, -32(%rbp)
	movsd	-32(%rbp), %xmm0                # xmm0 = mem[0],zero
	cvtsd2ss	%xmm0, %xmm0
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	get_time, .Lfunc_end2-get_time
	.cfi_endproc
                                        # -- End function
	.globl	initialize_matrix               # -- Begin function initialize_matrix
	.p2align	4, 0x90
	.type	initialize_matrix,@function
initialize_matrix:                      # @initialize_matrix
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movq	%rdx, -16(%rbp)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end3:
	.size	initialize_matrix, .Lfunc_end3-initialize_matrix
	.cfi_endproc
                                        # -- End function
	.globl	omp_potrf                       # -- Begin function omp_potrf
	.p2align	4, 0x90
	.type	omp_potrf,@function
omp_potrf:                              # @omp_potrf
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movl	%edx, -16(%rbp)
	movq	%rcx, -24(%rbp)
	movq	-8(%rbp), %rdx
	leaq	omp_potrf.L(%rip), %rdi
	leaq	-12(%rbp), %rsi
	leaq	-16(%rbp), %rcx
	leaq	omp_potrf.INFO(%rip), %r8
	callq	dpotrf_@PLT
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	omp_potrf, .Lfunc_end4-omp_potrf
	.cfi_endproc
                                        # -- End function
	.globl	omp_trsm                        # -- Begin function omp_trsm
	.p2align	4, 0x90
	.type	omp_trsm,@function
omp_trsm:                               # @omp_trsm
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%rbx
	subq	$72, %rsp
	.cfi_offset %rbx, -24
	movq	%rdi, -16(%rbp)
	movq	%rsi, -24(%rbp)
	movl	%edx, -28(%rbp)
	movl	%ecx, -32(%rbp)
	movq	%r8, -40(%rbp)
	movq	-16(%rbp), %r11
	movq	-24(%rbp), %r10
	leaq	omp_trsm.RI(%rip), %rdi
	leaq	omp_trsm.LO(%rip), %rsi
	leaq	omp_trsm.TR(%rip), %rdx
	leaq	omp_trsm.NU(%rip), %rcx
	leaq	-28(%rbp), %r9
	leaq	omp_trsm.DONE(%rip), %rbx
	leaq	-32(%rbp), %rax
	movq	%r9, %r8
	movq	%rbx, (%rsp)
	movq	%r11, 8(%rsp)
	movq	%rax, 16(%rsp)
	movq	%r10, 24(%rsp)
	movq	%rax, 32(%rsp)
	callq	dtrsm_@PLT
	addq	$72, %rsp
	popq	%rbx
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end5:
	.size	omp_trsm, .Lfunc_end5-omp_trsm
	.cfi_endproc
                                        # -- End function
	.globl	omp_syrk                        # -- Begin function omp_syrk
	.p2align	4, 0x90
	.type	omp_syrk,@function
omp_syrk:                               # @omp_syrk
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movl	%edx, -20(%rbp)
	movl	%ecx, -24(%rbp)
	movq	%r8, -32(%rbp)
	movq	-8(%rbp), %r9
	movq	-16(%rbp), %r10
	leaq	omp_syrk.LO(%rip), %rdi
	leaq	omp_syrk.NT(%rip), %rsi
	leaq	-20(%rbp), %rcx
	leaq	omp_syrk.DMONE(%rip), %r8
	leaq	-24(%rbp), %rax
	leaq	omp_syrk.DONE(%rip), %r11
	movq	%rcx, %rdx
	movq	%rax, (%rsp)
	movq	%r11, 8(%rsp)
	movq	%r10, 16(%rsp)
	movq	%rax, 24(%rsp)
	callq	dsyrk_@PLT
	addq	$64, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end6:
	.size	omp_syrk, .Lfunc_end6-omp_syrk
	.cfi_endproc
                                        # -- End function
	.globl	omp_gemm                        # -- Begin function omp_gemm
	.p2align	4, 0x90
	.type	omp_gemm,@function
omp_gemm:                               # @omp_gemm
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%r14
	pushq	%rbx
	subq	$96, %rsp
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	movq	%rdi, -24(%rbp)
	movq	%rsi, -32(%rbp)
	movq	%rdx, -40(%rbp)
	movl	%ecx, -44(%rbp)
	movl	%r8d, -48(%rbp)
	movq	%r9, -56(%rbp)
	movq	-24(%rbp), %r14
	movq	-32(%rbp), %rbx
	movq	-40(%rbp), %r10
	leaq	omp_gemm.NT(%rip), %rdi
	leaq	omp_gemm.TR(%rip), %rsi
	leaq	-44(%rbp), %r8
	leaq	omp_gemm.DMONE(%rip), %r9
	leaq	-48(%rbp), %rax
	leaq	omp_gemm.DONE(%rip), %r11
	movq	%r8, %rdx
	movq	%r8, %rcx
	movq	%r14, (%rsp)
	movq	%rax, 8(%rsp)
	movq	%rbx, 16(%rsp)
	movq	%rax, 24(%rsp)
	movq	%r11, 32(%rsp)
	movq	%r10, 40(%rsp)
	movq	%rax, 48(%rsp)
	callq	dgemm_@PLT
	addq	$96, %rsp
	popq	%rbx
	popq	%r14
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end7:
	.size	omp_gemm, .Lfunc_end7-omp_gemm
	.cfi_endproc
                                        # -- End function
	.globl	cholesky_blocked                # -- Begin function cholesky_blocked
	.p2align	4, 0x90
	.type	cholesky_blocked,@function
cholesky_blocked:                       # @cholesky_blocked
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movl	%edx, -20(%rbp)
	movl	NB(%rip), %eax
	movl	%eax, %ecx
	movl	NB(%rip), %eax
	movl	%eax, %r8d
	xorps	%xmm0, %xmm0
	movsd	%xmm0, -32(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$6, %esi
	leaq	cholesky_blocked.omp_outlined(%rip), %rdx
	leaq	-16(%rbp), %r9
	leaq	-20(%rbp), %r11
	leaq	-4(%rbp), %r10
	leaq	-32(%rbp), %rax
	movq	%r11, (%rsp)
	movq	%r10, 8(%rsp)
	movq	%rax, 16(%rsp)
	movb	$0, %al
	callq	__kmpc_fork_call@PLT
	addq	$64, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end8:
	.size	cholesky_blocked, .Lfunc_end8-cholesky_blocked
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function cholesky_blocked.omp_outlined
.LCPI9_0:
	.quad	0x408f400000000000              # double 1000
.LCPI9_1:
	.quad	0x412e848000000000              # double 1.0E+6
	.text
	.p2align	4, 0x90
	.type	cholesky_blocked.omp_outlined,@function
cholesky_blocked.omp_outlined:          # @cholesky_blocked.omp_outlined
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$656, %rsp                      # imm = 0x290
	movq	24(%rbp), %rax
	movq	16(%rbp), %rax
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	%r8, -40(%rbp)
	movq	%r9, -48(%rbp)
	movq	-24(%rbp), %rax
	movq	%rax, -600(%rbp)                # 8-byte Spill
	movq	-32(%rbp), %rax
	movq	%rax, -592(%rbp)                # 8-byte Spill
	movq	-40(%rbp), %rax
	movq	%rax, -584(%rbp)                # 8-byte Spill
	movq	-48(%rbp), %rax
	movq	%rax, -576(%rbp)                # 8-byte Spill
	movq	16(%rbp), %rax
	movq	%rax, -568(%rbp)                # 8-byte Spill
	movq	24(%rbp), %rax
	movq	%rax, -560(%rbp)                # 8-byte Spill
	movq	-8(%rbp), %rax
	movl	(%rax), %esi
	movl	%esi, -548(%rbp)                # 4-byte Spill
	leaq	.L__unnamed_1(%rip), %rdi
	callq	__kmpc_single@PLT
	cmpl	$0, %eax
	je	.LBB9_22
# %bb.1:                                # %omp_if.then
	movq	-584(%rbp), %rax                # 8-byte Reload
	movl	NB(%rip), %ecx
                                        # kill: def $rcx killed $ecx
	movq	%rcx, -616(%rbp)                # 8-byte Spill
	movl	NB(%rip), %ecx
                                        # kill: def $rcx killed $ecx
	movq	%rcx, -608(%rbp)                # 8-byte Spill
	movq	(%rax), %rax
	movq	%rax, -56(%rbp)
	movl	$0, -60(%rbp)
.LBB9_2:                                # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB9_4 Depth 2
                                        #       Child Loop BB9_6 Depth 3
                                        #       Child Loop BB9_10 Depth 3
                                        #         Child Loop BB9_12 Depth 4
	movq	-576(%rbp), %rcx                # 8-byte Reload
	movl	-60(%rbp), %eax
	cmpl	(%rcx), %eax
	jge	.LBB9_21
# %bb.3:                                # %for.body
                                        #   in Loop: Header=BB9_2 Depth=1
	movl	$1, %edi
	leaq	-80(%rbp), %rsi
	callq	clock_gettime@PLT
	movl	$0, -84(%rbp)
.LBB9_4:                                # %for.cond3
                                        #   Parent Loop BB9_2 Depth=1
                                        # =>  This Loop Header: Depth=2
                                        #       Child Loop BB9_6 Depth 3
                                        #       Child Loop BB9_10 Depth 3
                                        #         Child Loop BB9_12 Depth 4
	movl	-84(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB9_19
# %bb.5:                                # %for.body5
                                        #   in Loop: Header=BB9_4 Depth=2
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movq	-616(%rbp), %rcx                # 8-byte Reload
	movq	-568(%rbp), %rdx                # 8-byte Reload
	movq	-584(%rbp), %rdi                # 8-byte Reload
	movq	-592(%rbp), %r8                 # 8-byte Reload
	movq	-600(%rbp), %r9                 # 8-byte Reload
	movq	%r9, -136(%rbp)
	movq	%r8, -128(%rbp)
	movq	%rdi, -120(%rbp)
	movq	%rdx, -112(%rbp)
	movq	%rcx, -104(%rbp)
	movq	%rax, -96(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %edx
	movl	$56, %ecx
	movl	$48, %r8d
	leaq	.omp_task_entry.(%rip), %r9
	callq	__kmpc_omp_task_alloc@PLT
	movq	%rax, -624(%rbp)                # 8-byte Spill
	movq	(%rax), %rdi
	leaq	-136(%rbp), %rsi
	movl	$48, %edx
	callq	memcpy@PLT
	movq	-616(%rbp), %r9                 # 8-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-624(%rbp), %rdx                # 8-byte Reload
	movq	-56(%rbp), %rcx
	movq	%rcx, 40(%rdx)
	movl	-84(%rbp), %ecx
	movl	%ecx, 48(%rdx)
	leaq	-160(%rbp), %r8
	movq	-56(%rbp), %rcx
	movslq	-84(%rbp), %rdi
	imulq	%rax, %r9
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	movslq	-84(%rbp), %rdi
	imulq	%rax, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	shlq	$3, %rax
	movq	%rcx, -160(%rbp)
	movq	%rax, -152(%rbp)
	movb	$3, -144(%rbp)
	movq	$1, -168(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %ecx
	xorl	%r9d, %r9d
	xorl	%eax, %eax
                                        # kill: def $rax killed $eax
	movq	$0, (%rsp)
	callq	__kmpc_omp_task_with_deps@PLT
	movl	-84(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -172(%rbp)
.LBB9_6:                                # %for.cond8
                                        #   Parent Loop BB9_2 Depth=1
                                        #     Parent Loop BB9_4 Depth=2
                                        # =>    This Inner Loop Header: Depth=3
	movl	-172(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB9_9
# %bb.7:                                # %for.body10
                                        #   in Loop: Header=BB9_6 Depth=3
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movq	-616(%rbp), %rcx                # 8-byte Reload
	movq	-568(%rbp), %rdx                # 8-byte Reload
	movq	-584(%rbp), %rdi                # 8-byte Reload
	movq	-592(%rbp), %r8                 # 8-byte Reload
	movq	-600(%rbp), %r9                 # 8-byte Reload
	movq	%r9, -224(%rbp)
	movq	%r8, -216(%rbp)
	movq	%rdi, -208(%rbp)
	movq	%rdx, -200(%rbp)
	movq	%rcx, -192(%rbp)
	movq	%rax, -184(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %edx
	movl	$56, %ecx
	movl	$48, %r8d
	leaq	.omp_task_entry..3(%rip), %r9
	callq	__kmpc_omp_task_alloc@PLT
	movq	%rax, -632(%rbp)                # 8-byte Spill
	movq	(%rax), %rdi
	leaq	-224(%rbp), %rsi
	movl	$48, %edx
	callq	memcpy@PLT
	movq	-616(%rbp), %r9                 # 8-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-632(%rbp), %rdx                # 8-byte Reload
	movq	-56(%rbp), %rcx
	movq	%rcx, 40(%rdx)
	movl	-84(%rbp), %ecx
	movl	%ecx, 48(%rdx)
	movl	-172(%rbp), %ecx
	movl	%ecx, 52(%rdx)
	leaq	-272(%rbp), %r8
	movq	-56(%rbp), %rdi
	movslq	-84(%rbp), %rcx
	movq	%r9, %r10
	imulq	%rax, %r10
	imulq	%r10, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movslq	-84(%rbp), %rcx
	imulq	%rax, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movq	%rax, %rcx
	shlq	$3, %rcx
	movq	%rdi, -272(%rbp)
	movq	%rcx, -264(%rbp)
	movb	$1, -256(%rbp)
	movq	-56(%rbp), %rcx
	movslq	-84(%rbp), %rdi
	imulq	%rax, %r9
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	movslq	-172(%rbp), %rdi
	imulq	%rax, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	shlq	$3, %rax
	movq	%rcx, -248(%rbp)
	movq	%rax, -240(%rbp)
	movb	$3, -232(%rbp)
	movq	$2, -280(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$2, %ecx
	xorl	%r9d, %r9d
	xorl	%eax, %eax
                                        # kill: def $rax killed $eax
	movq	$0, (%rsp)
	callq	__kmpc_omp_task_with_deps@PLT
# %bb.8:                                # %for.inc
                                        #   in Loop: Header=BB9_6 Depth=3
	movl	-172(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -172(%rbp)
	jmp	.LBB9_6
.LBB9_9:                                # %for.end
                                        #   in Loop: Header=BB9_4 Depth=2
	movl	-84(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -284(%rbp)
.LBB9_10:                               # %for.cond23
                                        #   Parent Loop BB9_2 Depth=1
                                        #     Parent Loop BB9_4 Depth=2
                                        # =>    This Loop Header: Depth=3
                                        #         Child Loop BB9_12 Depth 4
	movl	-284(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB9_17
# %bb.11:                               # %for.body25
                                        #   in Loop: Header=BB9_10 Depth=3
	movl	-84(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -288(%rbp)
.LBB9_12:                               # %for.cond27
                                        #   Parent Loop BB9_2 Depth=1
                                        #     Parent Loop BB9_4 Depth=2
                                        #       Parent Loop BB9_10 Depth=3
                                        # =>      This Inner Loop Header: Depth=4
	movl	-288(%rbp), %eax
	cmpl	-284(%rbp), %eax
	jge	.LBB9_15
# %bb.13:                               # %for.body29
                                        #   in Loop: Header=BB9_12 Depth=4
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movq	-616(%rbp), %rcx                # 8-byte Reload
	movq	-568(%rbp), %rdx                # 8-byte Reload
	movq	-584(%rbp), %rdi                # 8-byte Reload
	movq	-592(%rbp), %r8                 # 8-byte Reload
	movq	-600(%rbp), %r9                 # 8-byte Reload
	movq	%r9, -336(%rbp)
	movq	%r8, -328(%rbp)
	movq	%rdi, -320(%rbp)
	movq	%rdx, -312(%rbp)
	movq	%rcx, -304(%rbp)
	movq	%rax, -296(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %edx
	movl	$64, %ecx
	movl	$48, %r8d
	leaq	.omp_task_entry..6(%rip), %r9
	callq	__kmpc_omp_task_alloc@PLT
	movq	%rax, -640(%rbp)                # 8-byte Spill
	movq	(%rax), %rdi
	leaq	-336(%rbp), %rsi
	movl	$48, %edx
	callq	memcpy@PLT
	movq	-616(%rbp), %r9                 # 8-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-640(%rbp), %rdx                # 8-byte Reload
	movq	-56(%rbp), %rcx
	movq	%rcx, 40(%rdx)
	movl	-84(%rbp), %ecx
	movl	%ecx, 48(%rdx)
	movl	-284(%rbp), %ecx
	movl	%ecx, 52(%rdx)
	movl	-288(%rbp), %ecx
	movl	%ecx, 56(%rdx)
	leaq	-408(%rbp), %r8
	movq	-56(%rbp), %rdi
	movslq	-84(%rbp), %rcx
	movq	%r9, %r10
	imulq	%rax, %r10
	imulq	%r10, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movslq	-284(%rbp), %rcx
	imulq	%rax, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movq	%rax, %rcx
	shlq	$3, %rcx
	movq	%rdi, -408(%rbp)
	movq	%rcx, -400(%rbp)
	movb	$1, -392(%rbp)
	movq	-56(%rbp), %rdi
	movslq	-84(%rbp), %rcx
	movq	%r9, %r10
	imulq	%rax, %r10
	imulq	%r10, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movslq	-288(%rbp), %rcx
	imulq	%rax, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movq	%rax, %rcx
	shlq	$3, %rcx
	movq	%rdi, -384(%rbp)
	movq	%rcx, -376(%rbp)
	movb	$1, -368(%rbp)
	movq	-56(%rbp), %rcx
	movslq	-288(%rbp), %rdi
	imulq	%rax, %r9
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	movslq	-284(%rbp), %rdi
	imulq	%rax, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	shlq	$3, %rax
	movq	%rcx, -360(%rbp)
	movq	%rax, -352(%rbp)
	movb	$3, -344(%rbp)
	movq	$3, -416(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$3, %ecx
	xorl	%r9d, %r9d
	xorl	%eax, %eax
                                        # kill: def $rax killed $eax
	movq	$0, (%rsp)
	callq	__kmpc_omp_task_with_deps@PLT
# %bb.14:                               # %for.inc45
                                        #   in Loop: Header=BB9_12 Depth=4
	movl	-288(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -288(%rbp)
	jmp	.LBB9_12
.LBB9_15:                               # %for.end47
                                        #   in Loop: Header=BB9_10 Depth=3
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movq	-616(%rbp), %rcx                # 8-byte Reload
	movq	-568(%rbp), %rdx                # 8-byte Reload
	movq	-584(%rbp), %rdi                # 8-byte Reload
	movq	-592(%rbp), %r8                 # 8-byte Reload
	movq	-600(%rbp), %r9                 # 8-byte Reload
	movq	%r9, -464(%rbp)
	movq	%r8, -456(%rbp)
	movq	%rdi, -448(%rbp)
	movq	%rdx, -440(%rbp)
	movq	%rcx, -432(%rbp)
	movq	%rax, -424(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$1, %edx
	movl	$56, %ecx
	movl	$48, %r8d
	leaq	.omp_task_entry..9(%rip), %r9
	callq	__kmpc_omp_task_alloc@PLT
	movq	%rax, -648(%rbp)                # 8-byte Spill
	movq	(%rax), %rdi
	leaq	-464(%rbp), %rsi
	movl	$48, %edx
	callq	memcpy@PLT
	movq	-616(%rbp), %r9                 # 8-byte Reload
	movq	-608(%rbp), %rax                # 8-byte Reload
	movl	-548(%rbp), %esi                # 4-byte Reload
	movq	-648(%rbp), %rdx                # 8-byte Reload
	movq	-56(%rbp), %rcx
	movq	%rcx, 40(%rdx)
	movl	-84(%rbp), %ecx
	movl	%ecx, 48(%rdx)
	movl	-284(%rbp), %ecx
	movl	%ecx, 52(%rdx)
	leaq	-512(%rbp), %r8
	movq	-56(%rbp), %rdi
	movslq	-84(%rbp), %rcx
	movq	%r9, %r10
	imulq	%rax, %r10
	imulq	%r10, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movslq	-284(%rbp), %rcx
	imulq	%rax, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rdi
	movq	%rax, %rcx
	shlq	$3, %rcx
	movq	%rdi, -512(%rbp)
	movq	%rcx, -504(%rbp)
	movb	$1, -496(%rbp)
	movq	-56(%rbp), %rcx
	movslq	-284(%rbp), %rdi
	imulq	%rax, %r9
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	movslq	-284(%rbp), %rdi
	imulq	%rax, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	shlq	$3, %rax
	movq	%rcx, -488(%rbp)
	movq	%rax, -480(%rbp)
	movb	$3, -472(%rbp)
	movq	$2, -520(%rbp)
	leaq	.L__unnamed_1(%rip), %rdi
	movl	$2, %ecx
	xorl	%r9d, %r9d
	xorl	%eax, %eax
                                        # kill: def $rax killed $eax
	movq	$0, (%rsp)
	callq	__kmpc_omp_task_with_deps@PLT
# %bb.16:                               # %for.inc59
                                        #   in Loop: Header=BB9_10 Depth=3
	movl	-284(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -284(%rbp)
	jmp	.LBB9_10
.LBB9_17:                               # %for.end61
                                        #   in Loop: Header=BB9_4 Depth=2
	jmp	.LBB9_18
.LBB9_18:                               # %for.inc62
                                        #   in Loop: Header=BB9_4 Depth=2
	movl	-84(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -84(%rbp)
	jmp	.LBB9_4
.LBB9_19:                               # %for.end64
                                        #   in Loop: Header=BB9_2 Depth=1
	movl	-548(%rbp), %esi                # 4-byte Reload
	leaq	.L__unnamed_1(%rip), %rdi
	callq	__kmpc_omp_taskwait@PLT
	movl	$1, %edi
	leaq	-536(%rbp), %rsi
	callq	clock_gettime@PLT
	movq	-560(%rbp), %rdx                # 8-byte Reload
	movq	-568(%rbp), %rcx                # 8-byte Reload
                                        # kill: def $esi killed $eax
	movq	-584(%rbp), %rax                # 8-byte Reload
	movq	-536(%rbp), %rsi
	subq	-80(%rbp), %rsi
	cvtsi2sd	%rsi, %xmm0
	movq	-528(%rbp), %rsi
	subq	-72(%rbp), %rsi
	cvtsi2sd	%rsi, %xmm1
	movsd	.LCPI9_1(%rip), %xmm2           # xmm2 = mem[0],zero
	divsd	%xmm2, %xmm1
	movsd	.LCPI9_0(%rip), %xmm2           # xmm2 = mem[0],zero
	mulsd	%xmm2, %xmm0
	addsd	%xmm1, %xmm0
	movsd	%xmm0, -544(%rbp)
	movsd	-544(%rbp), %xmm0               # xmm0 = mem[0],zero
	addsd	(%rdx), %xmm0
	movsd	%xmm0, (%rdx)
	movl	(%rcx), %edi
	movl	NB(%rip), %esi
	movq	(%rax), %r8
	movl	$2048, %edx                     # imm = 0x800
	leaq	original_matrix(%rip), %rcx
	callq	convert_to_blocks
# %bb.20:                               # %for.inc71
                                        #   in Loop: Header=BB9_2 Depth=1
	movl	-60(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -60(%rbp)
	jmp	.LBB9_2
.LBB9_21:                               # %for.end73
	movq	-560(%rbp), %rax                # 8-byte Reload
	movsd	(%rax), %xmm0                   # xmm0 = mem[0],zero
	leaq	.L.str(%rip), %rdi
	movb	$1, %al
	callq	printf@PLT
	movl	-548(%rbp), %esi                # 4-byte Reload
	leaq	.L__unnamed_1(%rip), %rdi
	callq	__kmpc_end_single@PLT
.LBB9_22:                               # %omp_if.end
	movl	-548(%rbp), %esi                # 4-byte Reload
	leaq	.L__unnamed_2(%rip), %rdi
	callq	__kmpc_barrier@PLT
	addq	$656, %rsp                      # imm = 0x290
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end9:
	.size	cholesky_blocked.omp_outlined, .Lfunc_end9-cholesky_blocked.omp_outlined
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_privates_map.
	.type	.omp_task_privates_map.,@function
.omp_task_privates_map.:                # @.omp_task_privates_map.
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	-8(%rbp), %rcx
	movq	-24(%rbp), %rax
	movq	%rcx, (%rax)
	addq	$8, %rcx
	movq	-16(%rbp), %rax
	movq	%rcx, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end10:
	.size	.omp_task_privates_map., .Lfunc_end10-.omp_task_privates_map.
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_entry.
	.type	.omp_task_entry.,@function
.omp_task_entry.:                       # @.omp_task_entry.
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$96, %rsp
	movl	%edi, -68(%rbp)
	movq	%rsi, -80(%rbp)
	movl	-68(%rbp), %edi
	movq	-80(%rbp), %rcx
	movq	%rcx, %rsi
	addq	$16, %rsi
	movq	(%rcx), %rax
	movq	%rcx, %rdx
	addq	$40, %rdx
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	leaq	.omp_task_privates_map.(%rip), %rdx
	movq	%rdx, -32(%rbp)
	movq	%rcx, -40(%rbp)
	movq	%rax, -48(%rbp)
	movq	-48(%rbp), %rax
	movq	%rax, -88(%rbp)                 # 8-byte Spill
	movq	8(%rax), %rax
	movq	%rax, -96(%rbp)                 # 8-byte Spill
	movq	-32(%rbp), %rax
	movq	-24(%rbp), %rdi
	leaq	-56(%rbp), %rsi
	leaq	-64(%rbp), %rdx
	callq	*%rax
	movq	-96(%rbp), %r8                  # 8-byte Reload
	movq	-88(%rbp), %rcx                 # 8-byte Reload
	movq	-56(%rbp), %rsi
	movq	-64(%rbp), %rax
	movq	16(%rcx), %rdx
	movq	(%rdx), %rdx
	movslq	(%rsi), %rdi
	imulq	%r8, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rdx
	movslq	(%rsi), %rsi
	movq	(%rdx,%rsi,8), %rdi
	movq	24(%rcx), %rdx
	movl	(%rdx), %esi
	movq	24(%rcx), %rcx
	movl	(%rcx), %edx
	movq	(%rax), %rcx
	callq	omp_potrf
	xorl	%eax, %eax
	addq	$96, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end11:
	.size	.omp_task_entry., .Lfunc_end11-.omp_task_entry.
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_privates_map..2
	.type	.omp_task_privates_map..2,@function
.omp_task_privates_map..2:              # @.omp_task_privates_map..2
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	-8(%rbp), %rcx
	movq	-32(%rbp), %rax
	movq	%rcx, (%rax)
	movq	%rcx, %rdx
	addq	$8, %rdx
	movq	-16(%rbp), %rax
	movq	%rdx, (%rax)
	addq	$12, %rcx
	movq	-24(%rbp), %rax
	movq	%rcx, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end12:
	.size	.omp_task_privates_map..2, .Lfunc_end12-.omp_task_privates_map..2
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_entry..3
	.type	.omp_task_entry..3,@function
.omp_task_entry..3:                     # @.omp_task_entry..3
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$112, %rsp
	movl	%edi, -76(%rbp)
	movq	%rsi, -88(%rbp)
	movl	-76(%rbp), %edi
	movq	-88(%rbp), %rcx
	movq	%rcx, %rsi
	addq	$16, %rsi
	movq	(%rcx), %rax
	movq	%rcx, %rdx
	addq	$40, %rdx
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	leaq	.omp_task_privates_map..2(%rip), %rdx
	movq	%rdx, -32(%rbp)
	movq	%rcx, -40(%rbp)
	movq	%rax, -48(%rbp)
	movq	-48(%rbp), %rax
	movq	%rax, -96(%rbp)                 # 8-byte Spill
	movq	8(%rax), %rax
	movq	%rax, -104(%rbp)                # 8-byte Spill
	movq	-32(%rbp), %rax
	movq	-24(%rbp), %rdi
	leaq	-56(%rbp), %rsi
	leaq	-64(%rbp), %rdx
	leaq	-72(%rbp), %rcx
	callq	*%rax
	movq	-104(%rbp), %r9                 # 8-byte Reload
	movq	-96(%rbp), %rcx                 # 8-byte Reload
	movq	-56(%rbp), %r8
	movq	-64(%rbp), %rsi
	movq	-72(%rbp), %rax
	movq	16(%rcx), %rdx
	movq	(%rdx), %rdx
	movslq	(%r8), %rdi
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rdx
	movslq	(%r8), %rdi
	movq	(%rdx,%rdi,8), %rdi
	movq	16(%rcx), %rdx
	movq	(%rdx), %rdx
	movslq	(%r8), %r8
	imulq	%r9, %r8
	shlq	$3, %r8
	addq	%r8, %rdx
	movslq	(%rsi), %rsi
	movq	(%rdx,%rsi,8), %rsi
	movq	24(%rcx), %rdx
	movl	(%rdx), %edx
	movq	24(%rcx), %rcx
	movl	(%rcx), %ecx
	movq	(%rax), %r8
	callq	omp_trsm
	xorl	%eax, %eax
	addq	$112, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end13:
	.size	.omp_task_entry..3, .Lfunc_end13-.omp_task_entry..3
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_privates_map..5
	.type	.omp_task_privates_map..5,@function
.omp_task_privates_map..5:              # @.omp_task_privates_map..5
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	%r8, -40(%rbp)
	movq	-8(%rbp), %rcx
	movq	-40(%rbp), %rax
	movq	%rcx, (%rax)
	movq	%rcx, %rdx
	addq	$8, %rdx
	movq	-16(%rbp), %rax
	movq	%rdx, (%rax)
	movq	%rcx, %rdx
	addq	$12, %rdx
	movq	-24(%rbp), %rax
	movq	%rdx, (%rax)
	addq	$16, %rcx
	movq	-32(%rbp), %rax
	movq	%rcx, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end14:
	.size	.omp_task_privates_map..5, .Lfunc_end14-.omp_task_privates_map..5
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_entry..6
	.type	.omp_task_entry..6,@function
.omp_task_entry..6:                     # @.omp_task_entry..6
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$112, %rsp
	movl	%edi, -84(%rbp)
	movq	%rsi, -96(%rbp)
	movl	-84(%rbp), %edi
	movq	-96(%rbp), %rcx
	movq	%rcx, %rsi
	addq	$16, %rsi
	movq	(%rcx), %rax
	movq	%rcx, %rdx
	addq	$40, %rdx
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	leaq	.omp_task_privates_map..5(%rip), %rdx
	movq	%rdx, -32(%rbp)
	movq	%rcx, -40(%rbp)
	movq	%rax, -48(%rbp)
	movq	-48(%rbp), %rax
	movq	%rax, -104(%rbp)                # 8-byte Spill
	movq	8(%rax), %rax
	movq	%rax, -112(%rbp)                # 8-byte Spill
	movq	-32(%rbp), %rax
	movq	-24(%rbp), %rdi
	leaq	-56(%rbp), %rsi
	leaq	-64(%rbp), %rdx
	leaq	-72(%rbp), %rcx
	leaq	-80(%rbp), %r8
	callq	*%rax
	movq	-112(%rbp), %r10                # 8-byte Reload
	movq	-104(%rbp), %r8                 # 8-byte Reload
	movq	-56(%rbp), %rsi
	movq	-64(%rbp), %rdx
	movq	-72(%rbp), %r9
	movq	-80(%rbp), %rax
	movq	16(%r8), %rcx
	movq	(%rcx), %rcx
	movslq	(%rsi), %rdi
	imulq	%r10, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rcx
	movslq	(%rdx), %rdi
	movq	(%rcx,%rdi,8), %rdi
	movq	16(%r8), %rcx
	movq	(%rcx), %rcx
	movslq	(%rsi), %rsi
	imulq	%r10, %rsi
	shlq	$3, %rsi
	addq	%rsi, %rcx
	movslq	(%r9), %rsi
	movq	(%rcx,%rsi,8), %rsi
	movq	16(%r8), %rcx
	movq	(%rcx), %rcx
	movslq	(%r9), %r9
	imulq	%r10, %r9
	shlq	$3, %r9
	addq	%r9, %rcx
	movslq	(%rdx), %rdx
	movq	(%rcx,%rdx,8), %rdx
	movq	24(%r8), %rcx
	movl	(%rcx), %ecx
	movq	24(%r8), %r8
	movl	(%r8), %r8d
	movq	(%rax), %r9
	callq	omp_gemm
	xorl	%eax, %eax
	addq	$112, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end15:
	.size	.omp_task_entry..6, .Lfunc_end15-.omp_task_entry..6
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_privates_map..8
	.type	.omp_task_privates_map..8,@function
.omp_task_privates_map..8:              # @.omp_task_privates_map..8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	%rcx, -32(%rbp)
	movq	-8(%rbp), %rcx
	movq	-32(%rbp), %rax
	movq	%rcx, (%rax)
	movq	%rcx, %rdx
	addq	$8, %rdx
	movq	-16(%rbp), %rax
	movq	%rdx, (%rax)
	addq	$12, %rcx
	movq	-24(%rbp), %rax
	movq	%rcx, (%rax)
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end16:
	.size	.omp_task_privates_map..8, .Lfunc_end16-.omp_task_privates_map..8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function .omp_task_entry..9
	.type	.omp_task_entry..9,@function
.omp_task_entry..9:                     # @.omp_task_entry..9
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$112, %rsp
	movl	%edi, -76(%rbp)
	movq	%rsi, -88(%rbp)
	movl	-76(%rbp), %edi
	movq	-88(%rbp), %rcx
	movq	%rcx, %rsi
	addq	$16, %rsi
	movq	(%rcx), %rax
	movq	%rcx, %rdx
	addq	$40, %rdx
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	leaq	.omp_task_privates_map..8(%rip), %rdx
	movq	%rdx, -32(%rbp)
	movq	%rcx, -40(%rbp)
	movq	%rax, -48(%rbp)
	movq	-48(%rbp), %rax
	movq	%rax, -96(%rbp)                 # 8-byte Spill
	movq	8(%rax), %rax
	movq	%rax, -104(%rbp)                # 8-byte Spill
	movq	-32(%rbp), %rax
	movq	-24(%rbp), %rdi
	leaq	-56(%rbp), %rsi
	leaq	-64(%rbp), %rdx
	leaq	-72(%rbp), %rcx
	callq	*%rax
	movq	-104(%rbp), %r9                 # 8-byte Reload
	movq	-96(%rbp), %rcx                 # 8-byte Reload
	movq	-56(%rbp), %rdi
	movq	-64(%rbp), %rsi
	movq	-72(%rbp), %rax
	movq	16(%rcx), %rdx
	movq	(%rdx), %rdx
	movslq	(%rdi), %rdi
	imulq	%r9, %rdi
	shlq	$3, %rdi
	addq	%rdi, %rdx
	movslq	(%rsi), %rdi
	movq	(%rdx,%rdi,8), %rdi
	movq	16(%rcx), %rdx
	movq	(%rdx), %rdx
	movslq	(%rsi), %r8
	imulq	%r9, %r8
	shlq	$3, %r8
	addq	%r8, %rdx
	movslq	(%rsi), %rsi
	movq	(%rdx,%rsi,8), %rsi
	movq	24(%rcx), %rdx
	movl	(%rdx), %edx
	movq	24(%rcx), %rcx
	movl	(%rcx), %ecx
	movq	(%rax), %r8
	callq	omp_syrk
	xorl	%eax, %eax
	addq	$112, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end17:
	.size	.omp_task_entry..9, .Lfunc_end17-.omp_task_entry..9
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function convert_to_blocks
	.type	convert_to_blocks,@function
convert_to_blocks:                      # @convert_to_blocks
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movl	%edx, -12(%rbp)
	movq	%rcx, -24(%rbp)
	movq	%r8, -32(%rbp)
	movl	-12(%rbp), %eax
                                        # kill: def $rax killed $eax
	movq	%rax, -56(%rbp)                 # 8-byte Spill
	movl	-8(%rbp), %eax
                                        # kill: def $rax killed $eax
	movq	%rax, -48(%rbp)                 # 8-byte Spill
	movl	$0, -36(%rbp)
.LBB18_1:                               # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB18_3 Depth 2
	movl	-36(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB18_8
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB18_1 Depth=1
	movl	$0, -40(%rbp)
.LBB18_3:                               # %for.cond1
                                        #   Parent Loop BB18_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-40(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB18_6
# %bb.4:                                # %for.body3
                                        #   in Loop: Header=BB18_3 Depth=2
	movq	-48(%rbp), %r8                  # 8-byte Reload
	movq	-56(%rbp), %rcx                 # 8-byte Reload
	movl	-12(%rbp), %edi
	movl	-4(%rbp), %esi
	movq	-24(%rbp), %rdx
	movl	-36(%rbp), %eax
	imull	-4(%rbp), %eax
	cltq
	imulq	%rcx, %rax
	shlq	$3, %rax
	addq	%rax, %rdx
	movl	-40(%rbp), %eax
	imull	-4(%rbp), %eax
	cltq
	shlq	$3, %rax
	addq	%rax, %rdx
	movq	-32(%rbp), %rax
	movslq	-36(%rbp), %rcx
	imulq	%r8, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rax
	movslq	-40(%rbp), %rcx
	movq	(%rax,%rcx,8), %rcx
	callq	gather_block
# %bb.5:                                # %for.inc
                                        #   in Loop: Header=BB18_3 Depth=2
	movl	-40(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -40(%rbp)
	jmp	.LBB18_3
.LBB18_6:                               # %for.end
                                        #   in Loop: Header=BB18_1 Depth=1
	jmp	.LBB18_7
.LBB18_7:                               # %for.inc11
                                        #   in Loop: Header=BB18_1 Depth=1
	movl	-36(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -36(%rbp)
	jmp	.LBB18_1
.LBB18_8:                               # %for.end13
	addq	$64, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end18:
	.size	convert_to_blocks, .Lfunc_end18-convert_to_blocks
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function gather_block
	.type	gather_block,@function
gather_block:                           # @gather_block
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movq	%rdx, -16(%rbp)
	movq	%rcx, -24(%rbp)
	movl	$0, -28(%rbp)
.LBB19_1:                               # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB19_3 Depth 2
	movl	-28(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB19_8
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB19_1 Depth=1
	movl	$0, -32(%rbp)
.LBB19_3:                               # %for.cond1
                                        #   Parent Loop BB19_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-32(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB19_6
# %bb.4:                                # %for.body3
                                        #   in Loop: Header=BB19_3 Depth=2
	movq	-16(%rbp), %rax
	movl	-28(%rbp), %ecx
	imull	-4(%rbp), %ecx
	addl	-32(%rbp), %ecx
	movslq	%ecx, %rcx
	movsd	(%rax,%rcx,8), %xmm0            # xmm0 = mem[0],zero
	movq	-24(%rbp), %rax
	movl	-28(%rbp), %ecx
	imull	-8(%rbp), %ecx
	addl	-32(%rbp), %ecx
	movslq	%ecx, %rcx
	movsd	%xmm0, (%rax,%rcx,8)
# %bb.5:                                # %for.inc
                                        #   in Loop: Header=BB19_3 Depth=2
	movl	-32(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -32(%rbp)
	jmp	.LBB19_3
.LBB19_6:                               # %for.end
                                        #   in Loop: Header=BB19_1 Depth=1
	jmp	.LBB19_7
.LBB19_7:                               # %for.inc8
                                        #   in Loop: Header=BB19_1 Depth=1
	movl	-28(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -28(%rbp)
	jmp	.LBB19_1
.LBB19_8:                               # %for.end10
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end19:
	.size	gather_block, .Lfunc_end19-gather_block
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$144, %rsp
	movl	$0, -4(%rbp)
	movl	%edi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	cmpl	$3, -8(%rbp)
	je	.LBB20_2
# %bb.1:                                # %if.then
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	leaq	.L.str.10(%rip), %rsi
	movb	$0, %al
	callq	fprintf@PLT
	movl	$1, %edi
	callq	exit@PLT
.LBB20_2:                               # %if.end
	movq	-16(%rbp), %rax
	movq	8(%rax), %rdi
	callq	atoi@PLT
	movl	%eax, NB(%rip)
	cmpl	$2048, NB(%rip)                 # imm = 0x800
	jle	.LBB20_4
# %bb.3:                                # %if.then3
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	movl	NB(%rip), %edx
	leaq	.L.str.11(%rip), %rsi
	movl	$2048, %ecx                     # imm = 0x800
	movb	$0, %al
	callq	fprintf@PLT
	movl	$4294967295, %edi               # imm = 0xFFFFFFFF
	callq	exit@PLT
.LBB20_4:                               # %if.end5
	movq	-16(%rbp), %rax
	movq	16(%rax), %rdi
	callq	atoi@PLT
	movl	%eax, -20(%rbp)
	cmpl	$0, -20(%rbp)
	jge	.LBB20_6
# %bb.5:                                # %if.then9
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	leaq	.L.str.12(%rip), %rsi
	movb	$0, %al
	callq	fprintf@PLT
	movl	$1, %edi
	callq	exit@PLT
.LBB20_6:                               # %if.end11
	movq	.L__const.main.result(%rip), %rax
	movq	%rax, -48(%rbp)
	movq	.L__const.main.result+8(%rip), %rax
	movq	%rax, -40(%rbp)
	movq	.L__const.main.result+16(%rip), %rax
	movq	%rax, -32(%rbp)
	movl	$157, %edi
	callq	BLAS_dfpinfo
	movsd	%xmm0, -56(%rbp)
	movl	$2048, -60(%rbp)                # imm = 0x800
	movl	$2048, %eax                     # imm = 0x800
	cltd
	idivl	NB(%rip)
	movl	%eax, -64(%rbp)
	movl	-64(%rbp), %esi
	leaq	.L.str.16(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
	movl	$0, -68(%rbp)
	movl	-64(%rbp), %esi
	movl	$2048, %edi                     # imm = 0x800
	leaq	matrix(%rip), %rdx
	callq	initialize_matrix
	movslq	NB(%rip), %rdi
	shlq	$3, %rdi
	callq	malloc@PLT
	movq	%rax, dummy(%rip)
	movl	$0, -72(%rbp)
.LBB20_7:                               # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	movl	-72(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB20_10
# %bb.8:                                # %for.body
                                        #   in Loop: Header=BB20_7 Depth=1
	movslq	NB(%rip), %rdi
	shlq	$3, %rdi
	callq	malloc@PLT
	movq	%rax, %rdx
	movq	dummy(%rip), %rax
	movslq	-72(%rbp), %rcx
	movq	%rdx, (%rax,%rcx,8)
# %bb.9:                                # %for.inc
                                        #   in Loop: Header=BB20_7 Depth=1
	movl	-72(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -72(%rbp)
	jmp	.LBB20_7
.LBB20_10:                              # %for.end
	movl	$0, -76(%rbp)
.LBB20_11:                              # %for.cond22
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB20_13 Depth 2
	movl	-76(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB20_18
# %bb.12:                               # %for.body25
                                        #   in Loop: Header=BB20_11 Depth=1
	movl	$0, -80(%rbp)
.LBB20_13:                              # %for.cond26
                                        #   Parent Loop BB20_11 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-80(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB20_16
# %bb.14:                               # %for.body29
                                        #   in Loop: Header=BB20_13 Depth=2
	movl	$2048, %eax                     # imm = 0x800
	cltd
	idivl	NB(%rip)
	movl	%eax, -120(%rbp)                # 4-byte Spill
	movl	$2048, %eax                     # imm = 0x800
	cltd
	idivl	NB(%rip)
	movl	%eax, %ecx
	movl	-120(%rbp), %eax                # 4-byte Reload
	imull	%ecx, %eax
	movslq	%eax, %rdi
	movl	$8, %esi
	callq	calloc@PLT
	movq	%rax, %rdx
	movq	dummy(%rip), %rax
	movslq	-76(%rbp), %rcx
	movq	(%rax,%rcx,8), %rax
	movslq	-80(%rbp), %rcx
	movq	%rdx, (%rax,%rcx,8)
# %bb.15:                               # %for.inc39
                                        #   in Loop: Header=BB20_13 Depth=2
	movl	-80(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -80(%rbp)
	jmp	.LBB20_13
.LBB20_16:                              # %for.end41
                                        #   in Loop: Header=BB20_11 Depth=1
	jmp	.LBB20_17
.LBB20_17:                              # %for.inc42
                                        #   in Loop: Header=BB20_11 Depth=1
	movl	-76(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -76(%rbp)
	jmp	.LBB20_11
.LBB20_18:                              # %for.end44
	movl	NB(%rip), %eax
	movl	%eax, %ecx
	movq	%rcx, %rax
	movq	%rax, -136(%rbp)                # 8-byte Spill
	movq	%rsp, %rdx
	movq	%rdx, -88(%rbp)
	movq	%rcx, %rdx
	imulq	%rdx, %rdx
	leaq	15(,%rdx,8), %rsi
	andq	$-16, %rsi
	movq	%rsp, %rdx
	subq	%rsi, %rdx
	movq	%rdx, -128(%rbp)                # 8-byte Spill
	movq	%rdx, %rsp
	movq	%rcx, -96(%rbp)
	movq	%rax, -104(%rbp)
	movl	$0, -108(%rbp)
.LBB20_19:                              # %for.cond46
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB20_21 Depth 2
	movl	-108(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB20_26
# %bb.20:                               # %for.body49
                                        #   in Loop: Header=BB20_19 Depth=1
	movl	$0, -112(%rbp)
.LBB20_21:                              # %for.cond51
                                        #   Parent Loop BB20_19 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-112(%rbp), %eax
	cmpl	NB(%rip), %eax
	jge	.LBB20_24
# %bb.22:                               # %for.body54
                                        #   in Loop: Header=BB20_21 Depth=2
	movq	-128(%rbp), %rax                # 8-byte Reload
	movq	-136(%rbp), %rsi                # 8-byte Reload
	movq	dummy(%rip), %rcx
	movslq	-108(%rbp), %rdx
	movq	(%rcx,%rdx,8), %rcx
	movslq	-112(%rbp), %rdx
	movq	(%rcx,%rdx,8), %rdx
	movslq	-108(%rbp), %rcx
	imulq	%rsi, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rax
	movslq	-112(%rbp), %rcx
	movq	%rdx, (%rax,%rcx,8)
# %bb.23:                               # %for.inc63
                                        #   in Loop: Header=BB20_21 Depth=2
	movl	-112(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -112(%rbp)
	jmp	.LBB20_21
.LBB20_24:                              # %for.end65
                                        #   in Loop: Header=BB20_19 Depth=1
	jmp	.LBB20_25
.LBB20_25:                              # %for.inc66
                                        #   in Loop: Header=BB20_19 Depth=1
	movl	-108(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -108(%rbp)
	jmp	.LBB20_19
.LBB20_26:                              # %for.end68
	movl	$0, -116(%rbp)
.LBB20_27:                              # %for.cond70
                                        # =>This Inner Loop Header: Depth=1
	cmpl	$4194304, -116(%rbp)            # imm = 0x400000
	jge	.LBB20_30
# %bb.28:                               # %for.body73
                                        #   in Loop: Header=BB20_27 Depth=1
	movslq	-116(%rbp), %rcx
	leaq	matrix(%rip), %rax
	movsd	(%rax,%rcx,8), %xmm0            # xmm0 = mem[0],zero
	movslq	-116(%rbp), %rcx
	leaq	original_matrix(%rip), %rax
	movsd	%xmm0, (%rax,%rcx,8)
# %bb.29:                               # %for.inc78
                                        #   in Loop: Header=BB20_27 Depth=1
	movl	-116(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -116(%rbp)
	jmp	.LBB20_27
.LBB20_30:                              # %for.end80
	movq	-128(%rbp), %r8                 # 8-byte Reload
	movl	-64(%rbp), %edi
	movl	NB(%rip), %esi
	movl	$2048, %edx                     # imm = 0x800
	leaq	matrix(%rip), %rcx
	callq	convert_to_blocks
	movq	-128(%rbp), %rsi                # 8-byte Reload
	movl	-64(%rbp), %edi
	movl	-20(%rbp), %edx
	callq	cholesky_blocked
	movq	-128(%rbp), %rcx                # 8-byte Reload
	movl	-64(%rbp), %edi
	movl	NB(%rip), %esi
	movl	$2048, %edx                     # imm = 0x800
	leaq	matrix(%rip), %r8
	callq	convert_to_linear
	movl	$0, -4(%rbp)
	movq	-88(%rbp), %rax
	movq	%rax, %rsp
	movl	-4(%rbp), %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end20:
	.size	main, .Lfunc_end20-main
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function BLAS_dfpinfo
.LCPI21_0:
	.quad	0x4000000000000000              # double 2
	.text
	.p2align	4, 0x90
	.type	BLAS_dfpinfo,@function
BLAS_dfpinfo:                           # @BLAS_dfpinfo
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$112, %rsp
	movl	%edi, -12(%rbp)
	movabsq	$4611686018427387904, %rax      # imm = 0x4000000000000000
	movq	%rax, -24(%rbp)
	movl	$53, -28(%rbp)
	movl	$1024, -32(%rbp)                # imm = 0x400
	movl	$-1021, -36(%rbp)               # imm = 0xFC03
	movabsq	$31356346816292452, %rax        # imm = 0x6F666E69706664
	movq	%rax, -51(%rbp)
	movabsq	$8099271340453874754, %rax      # imm = 0x7066645F53414C42
	movq	%rax, -56(%rbp)
	movsd	.LCPI21_0(%rip), %xmm0          # xmm0 = mem[0],zero
	movsd	%xmm0, -104(%rbp)               # 8-byte Spill
	movl	$-53, %edi
	callq	BLAS_dpow_di
	movaps	%xmm0, %xmm1
	movsd	-104(%rbp), %xmm0               # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movsd	%xmm1, -64(%rbp)
	movl	$-1022, %edi                    # imm = 0xFC02
	callq	BLAS_dpow_di
	movaps	%xmm0, %xmm1
	movsd	-104(%rbp), %xmm0               # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movsd	%xmm1, -72(%rbp)
	movabsq	$4607182418800017408, %rax      # imm = 0x3FF0000000000000
	movq	%rax, -80(%rbp)
	movsd	-64(%rbp), %xmm2                # xmm2 = mem[0],zero
	movsd	-80(%rbp), %xmm1                # xmm1 = mem[0],zero
	subsd	%xmm2, %xmm1
	movsd	%xmm1, -80(%rbp)
	movsd	-80(%rbp), %xmm1                # xmm1 = mem[0],zero
	movsd	%xmm1, -96(%rbp)                # 8-byte Spill
	movl	$1023, %edi                     # imm = 0x3FF
	callq	BLAS_dpow_di
	movaps	%xmm0, %xmm1
	movsd	-96(%rbp), %xmm0                # 8-byte Reload
                                        # xmm0 = mem[0],zero
	mulsd	%xmm1, %xmm0
	addsd	%xmm0, %xmm0
	movsd	%xmm0, -80(%rbp)
	movl	-12(%rbp), %eax
	movl	%eax, -84(%rbp)                 # 4-byte Spill
	subl	$157, %eax
	je	.LBB21_1
	jmp	.LBB21_6
.LBB21_6:                               # %entry
	movl	-84(%rbp), %eax                 # 4-byte Reload
	subl	$161, %eax
	je	.LBB21_2
	jmp	.LBB21_3
.LBB21_1:                               # %sw.bb
	movsd	-64(%rbp), %xmm0                # xmm0 = mem[0],zero
	movsd	%xmm0, -8(%rbp)
	jmp	.LBB21_5
.LBB21_2:                               # %sw.bb4
	movsd	-72(%rbp), %xmm0                # xmm0 = mem[0],zero
	movsd	%xmm0, -8(%rbp)
	jmp	.LBB21_5
.LBB21_3:                               # %sw.default
	leaq	-56(%rbp), %rdi
	movl	-12(%rbp), %edx
	movl	$4294967295, %esi               # imm = 0xFFFFFFFF
	xorl	%ecx, %ecx
	callq	BLAS_error
# %bb.4:                                # %sw.epilog
	xorps	%xmm0, %xmm0
	movsd	%xmm0, -8(%rbp)
.LBB21_5:                               # %return
	movsd	-8(%rbp), %xmm0                 # xmm0 = mem[0],zero
	addq	$112, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end21:
	.size	BLAS_dfpinfo, .Lfunc_end21-BLAS_dfpinfo
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function convert_to_linear
	.type	convert_to_linear,@function
convert_to_linear:                      # @convert_to_linear
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movl	%edx, -12(%rbp)
	movq	%rcx, -24(%rbp)
	movq	%r8, -32(%rbp)
	movl	-8(%rbp), %eax
                                        # kill: def $rax killed $eax
	movq	%rax, -56(%rbp)                 # 8-byte Spill
	movl	-12(%rbp), %eax
                                        # kill: def $rax killed $eax
	movq	%rax, -48(%rbp)                 # 8-byte Spill
	movl	$0, -36(%rbp)
.LBB22_1:                               # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB22_3 Depth 2
	movl	-36(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB22_8
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB22_1 Depth=1
	movl	$0, -40(%rbp)
.LBB22_3:                               # %for.cond1
                                        #   Parent Loop BB22_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-40(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB22_6
# %bb.4:                                # %for.body3
                                        #   in Loop: Header=BB22_3 Depth=2
	movq	-48(%rbp), %r8                  # 8-byte Reload
	movq	-56(%rbp), %rdx                 # 8-byte Reload
	movl	-12(%rbp), %edi
	movl	-4(%rbp), %esi
	movq	-24(%rbp), %rax
	movslq	-36(%rbp), %rcx
	imulq	%rdx, %rcx
	shlq	$3, %rcx
	addq	%rcx, %rax
	movslq	-40(%rbp), %rcx
	movq	(%rax,%rcx,8), %rdx
	movq	-32(%rbp), %rcx
	movl	-36(%rbp), %eax
	imull	-4(%rbp), %eax
	cltq
	imulq	%r8, %rax
	shlq	$3, %rax
	addq	%rax, %rcx
	movl	-40(%rbp), %eax
	imull	-4(%rbp), %eax
	cltq
	shlq	$3, %rax
	addq	%rax, %rcx
	callq	scatter_block
# %bb.5:                                # %for.inc
                                        #   in Loop: Header=BB22_3 Depth=2
	movl	-40(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -40(%rbp)
	jmp	.LBB22_3
.LBB22_6:                               # %for.end
                                        #   in Loop: Header=BB22_1 Depth=1
	jmp	.LBB22_7
.LBB22_7:                               # %for.inc11
                                        #   in Loop: Header=BB22_1 Depth=1
	movl	-36(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -36(%rbp)
	jmp	.LBB22_1
.LBB22_8:                               # %for.end13
	addq	$64, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end22:
	.size	convert_to_linear, .Lfunc_end22-convert_to_linear
	.cfi_endproc
                                        # -- End function
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3, 0x0                          # -- Begin function BLAS_dpow_di
.LCPI23_0:
	.quad	0x3ff0000000000000              # double 1
	.text
	.p2align	4, 0x90
	.type	BLAS_dpow_di,@function
BLAS_dpow_di:                           # @BLAS_dpow_di
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movsd	%xmm0, -8(%rbp)
	movl	%edi, -12(%rbp)
	movsd	.LCPI23_0(%rip), %xmm0          # xmm0 = mem[0],zero
	movsd	%xmm0, -24(%rbp)
	cmpl	$0, -12(%rbp)
	jge	.LBB23_2
# %bb.1:                                # %if.then
	xorl	%eax, %eax
	subl	-12(%rbp), %eax
	movl	%eax, -12(%rbp)
	movsd	.LCPI23_0(%rip), %xmm0          # xmm0 = mem[0],zero
	divsd	-8(%rbp), %xmm0
	movsd	%xmm0, -8(%rbp)
.LBB23_2:                               # %if.end
	jmp	.LBB23_3
.LBB23_3:                               # %for.cond
                                        # =>This Inner Loop Header: Depth=1
	cmpl	$0, -12(%rbp)
	je	.LBB23_8
# %bb.4:                                # %for.body
                                        #   in Loop: Header=BB23_3 Depth=1
	movl	-12(%rbp), %eax
	andl	$1, %eax
	cmpl	$0, %eax
	je	.LBB23_6
# %bb.5:                                # %if.then2
                                        #   in Loop: Header=BB23_3 Depth=1
	movsd	-8(%rbp), %xmm0                 # xmm0 = mem[0],zero
	mulsd	-24(%rbp), %xmm0
	movsd	%xmm0, -24(%rbp)
.LBB23_6:                               # %if.end3
                                        #   in Loop: Header=BB23_3 Depth=1
	jmp	.LBB23_7
.LBB23_7:                               # %for.inc
                                        #   in Loop: Header=BB23_3 Depth=1
	movl	-12(%rbp), %eax
	sarl	%eax
	movl	%eax, -12(%rbp)
	movsd	-8(%rbp), %xmm0                 # xmm0 = mem[0],zero
	mulsd	-8(%rbp), %xmm0
	movsd	%xmm0, -8(%rbp)
	jmp	.LBB23_3
.LBB23_8:                               # %for.end
	movsd	-24(%rbp), %xmm0                # xmm0 = mem[0],zero
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end23:
	.size	BLAS_dpow_di, .Lfunc_end23-BLAS_dpow_di
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function BLAS_error
	.type	BLAS_error,@function
BLAS_error:                             # @BLAS_error
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movl	%esi, -12(%rbp)
	movl	%edx, -16(%rbp)
	movl	%ecx, -20(%rbp)
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	movq	-8(%rbp), %rdx
	movl	-12(%rbp), %ecx
	movl	-16(%rbp), %r8d
	movl	-20(%rbp), %r9d
	leaq	.L.str.17(%rip), %rsi
	movb	$0, %al
	callq	fprintf@PLT
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end24:
	.size	BLAS_error, .Lfunc_end24-BLAS_error
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function scatter_block
	.type	scatter_block,@function
scatter_block:                          # @scatter_block
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movq	%rdx, -16(%rbp)
	movq	%rcx, -24(%rbp)
	movl	$0, -28(%rbp)
.LBB25_1:                               # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB25_3 Depth 2
	movl	-28(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB25_8
# %bb.2:                                # %for.body
                                        #   in Loop: Header=BB25_1 Depth=1
	movl	$0, -32(%rbp)
.LBB25_3:                               # %for.cond1
                                        #   Parent Loop BB25_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	-32(%rbp), %eax
	cmpl	-8(%rbp), %eax
	jge	.LBB25_6
# %bb.4:                                # %for.body3
                                        #   in Loop: Header=BB25_3 Depth=2
	movq	-16(%rbp), %rax
	movl	-28(%rbp), %ecx
	imull	-8(%rbp), %ecx
	addl	-32(%rbp), %ecx
	movslq	%ecx, %rcx
	movsd	(%rax,%rcx,8), %xmm0            # xmm0 = mem[0],zero
	movq	-24(%rbp), %rax
	movl	-28(%rbp), %ecx
	imull	-4(%rbp), %ecx
	addl	-32(%rbp), %ecx
	movslq	%ecx, %rcx
	movsd	%xmm0, (%rax,%rcx,8)
# %bb.5:                                # %for.inc
                                        #   in Loop: Header=BB25_3 Depth=2
	movl	-32(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -32(%rbp)
	jmp	.LBB25_3
.LBB25_6:                               # %for.end
                                        #   in Loop: Header=BB25_1 Depth=1
	jmp	.LBB25_7
.LBB25_7:                               # %for.inc8
                                        #   in Loop: Header=BB25_1 Depth=1
	movl	-28(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -28(%rbp)
	jmp	.LBB25_1
.LBB25_8:                               # %for.end10
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end25:
	.size	scatter_block, .Lfunc_end25-scatter_block
	.cfi_endproc
                                        # -- End function
	.type	NB,@object                      # @NB
	.bss
	.globl	NB
	.p2align	2, 0x0
NB:
	.long	0                               # 0x0
	.size	NB, 4

	.type	dummy,@object                   # @dummy
	.globl	dummy
	.p2align	3, 0x0
dummy:
	.quad	0
	.size	dummy, 8

	.type	get_time.gtod_ref_time_sec,@object # @get_time.gtod_ref_time_sec
	.local	get_time.gtod_ref_time_sec
	.comm	get_time.gtod_ref_time_sec,8,8
	.type	omp_potrf.INFO,@object          # @omp_potrf.INFO
	.local	omp_potrf.INFO
	.comm	omp_potrf.INFO,4,4
	.type	omp_potrf.L,@object             # @omp_potrf.L
	.section	.rodata,"a",@progbits
omp_potrf.L:
	.byte	76                              # 0x4c
	.size	omp_potrf.L, 1

	.type	omp_trsm.LO,@object             # @omp_trsm.LO
	.data
omp_trsm.LO:
	.byte	76                              # 0x4c
	.size	omp_trsm.LO, 1

	.type	omp_trsm.TR,@object             # @omp_trsm.TR
omp_trsm.TR:
	.byte	84                              # 0x54
	.size	omp_trsm.TR, 1

	.type	omp_trsm.NU,@object             # @omp_trsm.NU
omp_trsm.NU:
	.byte	78                              # 0x4e
	.size	omp_trsm.NU, 1

	.type	omp_trsm.RI,@object             # @omp_trsm.RI
omp_trsm.RI:
	.byte	82                              # 0x52
	.size	omp_trsm.RI, 1

	.type	omp_trsm.DONE,@object           # @omp_trsm.DONE
	.p2align	3, 0x0
omp_trsm.DONE:
	.quad	0x3ff0000000000000              # double 1
	.size	omp_trsm.DONE, 8

	.type	omp_syrk.LO,@object             # @omp_syrk.LO
omp_syrk.LO:
	.byte	76                              # 0x4c
	.size	omp_syrk.LO, 1

	.type	omp_syrk.NT,@object             # @omp_syrk.NT
omp_syrk.NT:
	.byte	78                              # 0x4e
	.size	omp_syrk.NT, 1

	.type	omp_syrk.DONE,@object           # @omp_syrk.DONE
	.p2align	3, 0x0
omp_syrk.DONE:
	.quad	0x3ff0000000000000              # double 1
	.size	omp_syrk.DONE, 8

	.type	omp_syrk.DMONE,@object          # @omp_syrk.DMONE
	.p2align	3, 0x0
omp_syrk.DMONE:
	.quad	0xbff0000000000000              # double -1
	.size	omp_syrk.DMONE, 8

	.type	omp_gemm.TR,@object             # @omp_gemm.TR
	.section	.rodata,"a",@progbits
omp_gemm.TR:
	.byte	84                              # 0x54
	.size	omp_gemm.TR, 1

	.type	omp_gemm.NT,@object             # @omp_gemm.NT
omp_gemm.NT:
	.byte	78                              # 0x4e
	.size	omp_gemm.NT, 1

	.type	omp_gemm.DONE,@object           # @omp_gemm.DONE
	.data
	.p2align	3, 0x0
omp_gemm.DONE:
	.quad	0x3ff0000000000000              # double 1
	.size	omp_gemm.DONE, 8

	.type	omp_gemm.DMONE,@object          # @omp_gemm.DMONE
	.p2align	3, 0x0
omp_gemm.DMONE:
	.quad	0xbff0000000000000              # double -1
	.size	omp_gemm.DMONE, 8

	.type	.L__unnamed_3,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_3:
	.asciz	";unknown;unknown;0;0;;"
	.size	.L__unnamed_3, 23

	.type	.L__unnamed_1,@object           # @1
	.section	.data.rel.ro,"aw",@progbits
	.p2align	3, 0x0
.L__unnamed_1:
	.long	0                               # 0x0
	.long	2                               # 0x2
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_3
	.size	.L__unnamed_1, 24

	.type	original_matrix,@object         # @original_matrix
	.local	original_matrix
	.comm	original_matrix,33554432,16
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"%g ms passed\n"
	.size	.L.str, 14

	.type	.L__unnamed_2,@object           # @2
	.section	.data.rel.ro,"aw",@progbits
	.p2align	3, 0x0
.L__unnamed_2:
	.long	0                               # 0x0
	.long	322                             # 0x142
	.long	0                               # 0x0
	.long	22                              # 0x16
	.quad	.L__unnamed_3
	.size	.L__unnamed_2, 24

	.type	.L.str.10,@object               # @.str.10
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str.10:
	.asciz	"Usage: <block number> <num_iterations> \n"
	.size	.L.str.10, 41

	.type	.L.str.11,@object               # @.str.11
.L.str.11:
	.asciz	"NB = %d, DIM = %d, NB must be smaller than DIM\n"
	.size	.L.str.11, 48

	.type	.L.str.12,@object               # @.str.12
.L.str.12:
	.asciz	"num_iter must be positive\n"
	.size	.L.str.12, 27

	.type	.L.str.13,@object               # @.str.13
.L.str.13:
	.asciz	"n/a"
	.size	.L.str.13, 4

	.type	.L.str.14,@object               # @.str.14
.L.str.14:
	.asciz	"sucessful"
	.size	.L.str.14, 10

	.type	.L.str.15,@object               # @.str.15
.L.str.15:
	.asciz	"UNSUCCESSFUL"
	.size	.L.str.15, 13

	.type	.L__const.main.result,@object   # @__const.main.result
	.section	.data.rel.ro,"aw",@progbits
	.p2align	4, 0x0
.L__const.main.result:
	.quad	.L.str.13
	.quad	.L.str.14
	.quad	.L.str.15
	.size	.L__const.main.result, 24

	.type	.L.str.16,@object               # @.str.16
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str.16:
	.asciz	"ts = %d\n"
	.size	.L.str.16, 9

	.type	matrix,@object                  # @matrix
	.local	matrix
	.comm	matrix,33554432,16
	.type	.L__const.BLAS_dfpinfo.rname,@object # @__const.BLAS_dfpinfo.rname
.L__const.BLAS_dfpinfo.rname:
	.asciz	"BLAS_dfpinfo"
	.size	.L__const.BLAS_dfpinfo.rname, 13

	.type	.L.str.17,@object               # @.str.17
.L.str.17:
	.asciz	"%s %d %d %d\n"
	.size	.L.str.17, 13

	.ident	"clang version 18.0.0 (https://github.com/llvm/llvm-project.git 26eb4285b56edd8c897642078d91f16ff0fd3472)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym gettimeofday
	.addrsig_sym initialize_matrix
	.addrsig_sym omp_potrf
	.addrsig_sym dpotrf_
	.addrsig_sym omp_trsm
	.addrsig_sym dtrsm_
	.addrsig_sym omp_syrk
	.addrsig_sym dsyrk_
	.addrsig_sym omp_gemm
	.addrsig_sym dgemm_
	.addrsig_sym cholesky_blocked
	.addrsig_sym cholesky_blocked.omp_outlined
	.addrsig_sym __kmpc_single
	.addrsig_sym __kmpc_end_single
	.addrsig_sym clock_gettime
	.addrsig_sym .omp_task_privates_map.
	.addrsig_sym .omp_task_entry.
	.addrsig_sym __kmpc_omp_task_alloc
	.addrsig_sym __kmpc_omp_task_with_deps
	.addrsig_sym .omp_task_privates_map..2
	.addrsig_sym .omp_task_entry..3
	.addrsig_sym .omp_task_privates_map..5
	.addrsig_sym .omp_task_entry..6
	.addrsig_sym .omp_task_privates_map..8
	.addrsig_sym .omp_task_entry..9
	.addrsig_sym __kmpc_omp_taskwait
	.addrsig_sym convert_to_blocks
	.addrsig_sym printf
	.addrsig_sym __kmpc_barrier
	.addrsig_sym __kmpc_fork_call
	.addrsig_sym gather_block
	.addrsig_sym fprintf
	.addrsig_sym exit
	.addrsig_sym atoi
	.addrsig_sym BLAS_dfpinfo
	.addrsig_sym malloc
	.addrsig_sym calloc
	.addrsig_sym convert_to_linear
	.addrsig_sym BLAS_dpow_di
	.addrsig_sym BLAS_error
	.addrsig_sym scatter_block
	.addrsig_sym NB
	.addrsig_sym dummy
	.addrsig_sym get_time.gtod_ref_time_sec
	.addrsig_sym omp_potrf.INFO
	.addrsig_sym omp_potrf.L
	.addrsig_sym omp_trsm.LO
	.addrsig_sym omp_trsm.TR
	.addrsig_sym omp_trsm.NU
	.addrsig_sym omp_trsm.RI
	.addrsig_sym omp_trsm.DONE
	.addrsig_sym omp_syrk.LO
	.addrsig_sym omp_syrk.NT
	.addrsig_sym omp_syrk.DONE
	.addrsig_sym omp_syrk.DMONE
	.addrsig_sym omp_gemm.TR
	.addrsig_sym omp_gemm.NT
	.addrsig_sym omp_gemm.DONE
	.addrsig_sym omp_gemm.DMONE
	.addrsig_sym original_matrix
	.addrsig_sym stderr
	.addrsig_sym matrix
