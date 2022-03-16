/* Copyright (C) 2001-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H	1

#include <features.h>

#include <bits/types.h>
#include <bits/types/sigset_t.h>
#include <bits/types/stack_t.h>


#ifdef __USE_MISC
# define __ctx(fld) fld
#else
# define __ctx(fld) __ ## fld
#endif

#ifdef __x86_64__

/* Type for general register.  */
__extension__ typedef long long int greg_t;

/* Number of general registers.  */
#define __NGREG	23
#ifdef __USE_MISC
# define NGREG	__NGREG
#endif

/* Container for all general registers.  */
// 装载所有寄存器
typedef greg_t gregset_t[__NGREG];

#ifdef __USE_GNU
/* Number of each register in the `gregset_t' array.  */
enum
{
  REG_R8 = 0,
# define REG_R8		REG_R8
  REG_R9,
# define REG_R9		REG_R9
  REG_R10,
# define REG_R10	REG_R10
  REG_R11,
# define REG_R11	REG_R11
  REG_R12,
# define REG_R12	REG_R12
  REG_R13,
# define REG_R13	REG_R13
  REG_R14,
# define REG_R14	REG_R14
  REG_R15,
# define REG_R15	REG_R15
  REG_RDI,
# define REG_RDI	REG_RDI
  REG_RSI,
# define REG_RSI	REG_RSI
  REG_RBP,
# define REG_RBP	REG_RBP
  REG_RBX,
# define REG_RBX	REG_RBX
  REG_RDX,
# define REG_RDX	REG_RDX
  REG_RAX,
# define REG_RAX	REG_RAX
  REG_RCX,
# define REG_RCX	REG_RCX
  REG_RSP,
# define REG_RSP	REG_RSP
  REG_RIP,
# define REG_RIP	REG_RIP
  REG_EFL,
# define REG_EFL	REG_EFL
  REG_CSGSFS,		/* Actually short cs, gs, fs, __pad0.  */
# define REG_CSGSFS	REG_CSGSFS
  REG_ERR,
# define REG_ERR	REG_ERR
  REG_TRAPNO,
# define REG_TRAPNO	REG_TRAPNO
  REG_OLDMASK,
# define REG_OLDMASK	REG_OLDMASK
  REG_CR2
# define REG_CR2	REG_CR2
};
#endif

struct _libc_fpxreg
{
  unsigned short int __ctx(significand)[4];
  unsigned short int __ctx(exponent);
  unsigned short int __glibc_reserved1[3];
};

struct _libc_xmmreg
{
  __uint32_t	__ctx(element)[4];
};

struct _libc_fpstate
{
  /* 64-bit FXSAVE format.  */
  __uint16_t		__ctx(cwd);
  __uint16_t		__ctx(swd);
  __uint16_t		__ctx(ftw);
  __uint16_t		__ctx(fop);
  __uint64_t		__ctx(rip);
  __uint64_t		__ctx(rdp);
  __uint32_t		__ctx(mxcsr);
  __uint32_t		__ctx(mxcr_mask);
  struct _libc_fpxreg	_st[8];
  struct _libc_xmmreg	_xmm[16];
  __uint32_t		__glibc_reserved1[24];
};

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

/* Context to describe whole processor state.  */
typedef struct
  {
    gregset_t __ctx(gregs);         // 装载所有寄存器   
    /* Note that fpregs is a pointer.  */
    fpregset_t __ctx(fpregs);       // 存储所有寄存器类型
    __extension__ unsigned long long __reserved1 [8];
} mcontext_t;

/* Userlevel context.  */
typedef struct ucontext_t
  {
    unsigned long int           __ctx(uc_flags);
    struct ucontext_t *         uc_link;        // 指向下一个上下文
    stack_t                     uc_stack;       // 指向上下文使用的栈       
    mcontext_t                  uc_mcontext;    // 保存寄存器相关信息
    sigset_t                    uc_sigmask;
    struct _libc_fpstate        __fpregs_mem;
    __extension__ unsigned long long int __ssp[4];
  } ucontext_t;

#else /* !__x86_64__ */

/* Type for general register.  */
typedef int greg_t;

/* Number of general registers.  */
#define __NGREG	19
#ifdef __USE_MISC
# define NGREG	__NGREG
#endif

/* Container for all general registers.  */
typedef greg_t gregset_t[__NGREG];

#ifdef __USE_GNU
/* Number of each register is the `gregset_t' array.  */
enum
{
  REG_GS = 0,
# define REG_GS		REG_GS
  REG_FS,
# define REG_FS		REG_FS
  REG_ES,
# define REG_ES		REG_ES
  REG_DS,
# define REG_DS		REG_DS
  REG_EDI,
# define REG_EDI	REG_EDI
  REG_ESI,
# define REG_ESI	REG_ESI
  REG_EBP,
# define REG_EBP	REG_EBP
  REG_ESP,
# define REG_ESP	REG_ESP
  REG_EBX,
# define REG_EBX	REG_EBX
  REG_EDX,
# define REG_EDX	REG_EDX
  REG_ECX,
# define REG_ECX	REG_ECX
  REG_EAX,
# define REG_EAX	REG_EAX
  REG_TRAPNO,
# define REG_TRAPNO	REG_TRAPNO
  REG_ERR,
# define REG_ERR	REG_ERR
  REG_EIP,
# define REG_EIP	REG_EIP
  REG_CS,
# define REG_CS		REG_CS
  REG_EFL,
# define REG_EFL	REG_EFL
  REG_UESP,
# define REG_UESP	REG_UESP
  REG_SS
# define REG_SS	REG_SS
};
#endif

/* Definitions taken from the kernel headers.  */
struct _libc_fpreg
{
  unsigned short int __ctx(significand)[4];
  unsigned short int __ctx(exponent);
};

struct _libc_fpstate
{
  unsigned long int __ctx(cw);
  unsigned long int __ctx(sw);
  unsigned long int __ctx(tag);
  unsigned long int __ctx(ipoff);
  unsigned long int __ctx(cssel);
  unsigned long int __ctx(dataoff);
  unsigned long int __ctx(datasel);
  struct _libc_fpreg _st[8];
  unsigned long int __ctx(status);
};

/* Structure to describe FPU registers.  */
typedef struct _libc_fpstate *fpregset_t;

/* Context to describe whole processor state.  */
typedef struct
  {
    gregset_t __ctx(gregs);
    /* Due to Linux's history we have to use a pointer here.  The SysV/i386
       ABI requires a struct with the values.  */
    fpregset_t __ctx(fpregs);
    unsigned long int __ctx(oldmask);
    unsigned long int __ctx(cr2);
  } mcontext_t;

/* Userlevel context.  */
typedef struct ucontext_t
  {
    unsigned long int __ctx(uc_flags);
    struct ucontext_t *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
    struct _libc_fpstate __fpregs_mem;
    unsigned long int __ssp[4];
  } ucontext_t;

#endif /* !__x86_64__ */

#undef __ctx

#endif /* sys/ucontext.h */

/*  int __setcontext (const ucontext_t *ucp)

  Restores the machine context in UCP and thereby resumes execution
  in that context.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to restore anything
  other than the PRESERVED state.  */

ENTRY(__setcontext)
    /* Save argument since syscall will destroy it.  */
    pushq   %rdi
    cfi_adjust_cfa_offset(8)

    /* Set the signal mask with
       rt_sigprocmask (SIG_SETMASK, mask, NULL, _NSIG/8).  */
    leaq    oSIGMASK(%rdi), %rsi
    xorl    %edx, %edx
    movl    $SIG_SETMASK, %edi
    movl    $_NSIG8,%r10d
    movl    $__NR_rt_sigprocmask, %eax
    syscall
    popq    %rdi            /* Reload %rdi, adjust stack.  */
    cfi_adjust_cfa_offset(-8)
    cmpq    $-4095, %rax        /* Check %rax for error.  */
    jae SYSCALL_ERROR_LABEL /* Jump to error handler if error.  */

    /* Restore the floating-point context.  Not the registers, only the
       rest.  */
    movq    oFPREGS(%rdi), %rcx
    fldenv  (%rcx)
    ldmxcsr oMXCSR(%rdi)


    /* Load the new stack pointer, the preserved registers and
       registers used for passing args.  */
    cfi_def_cfa(%rdi, 0)
    cfi_offset(%rbx,oRBX)
    cfi_offset(%rbp,oRBP)
    cfi_offset(%r12,oR12)
    cfi_offset(%r13,oR13)
    cfi_offset(%r14,oR14)
    cfi_offset(%r15,oR15)
    cfi_offset(%rsp,oRSP)
    cfi_offset(%rip,oRIP)

    movq    oRSP(%rdi), %rsp
    movq    oRBX(%rdi), %rbx
    movq    oRBP(%rdi), %rbp
    movq    oR12(%rdi), %r12
    movq    oR13(%rdi), %r13
    movq    oR14(%rdi), %r14
    movq    oR15(%rdi), %r15

    /* The following ret should return to the address set with
    getcontext.  Therefore push the address on the stack.  */
    movq    oRIP(%rdi), %rcx
    pushq   %rcx

    movq    oRSI(%rdi), %rsi
    movq    oRDX(%rdi), %rdx
    movq    oRCX(%rdi), %rcx
    movq    oR8(%rdi), %r8
    movq    oR9(%rdi), %r9

    /* Setup finally  %rdi.  */
    movq    oRDI(%rdi), %rdi

    /* End FDE here, we fall into another context.  */
    cfi_endproc
    cfi_startproc

    /* Clear rax to indicate success.  */
    xorl    %eax, %eax
    ret
PSEUDO_END(__setcontext)

/*  int __getcontext (ucontext_t *ucp)

  Saves the machine context in UCP such that when it is activated,
  it appears as if __getcontext() returned again.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to save anything
  other than the PRESERVED state.  */


ENTRY(__getcontext)
    /* Save the preserved registers, the registers used for passing
       args, and the return address.  */
    movq    %rbx, oRBX(%rdi)
    movq    %rbp, oRBP(%rdi)
    movq    %r12, oR12(%rdi)
    movq    %r13, oR13(%rdi)
    movq    %r14, oR14(%rdi)
    movq    %r15, oR15(%rdi)

    movq    %rdi, oRDI(%rdi)
    movq    %rsi, oRSI(%rdi)
    movq    %rdx, oRDX(%rdi)
    movq    %rcx, oRCX(%rdi)
    movq    %r8, oR8(%rdi)
    movq    %r9, oR9(%rdi)

    movq    (%rsp), %rcx
    movq    %rcx, oRIP(%rdi)
    leaq    8(%rsp), %rcx       /* Exclude the return address.  */
    movq    %rcx, oRSP(%rdi)

    /* We have separate floating-point register content memory on the
       stack.  We use the __fpregs_mem block in the context.  Set the
       links up correctly.  */

    leaq    oFPREGSMEM(%rdi), %rcx
    movq    %rcx, oFPREGS(%rdi)
    /* Save the floating-point environment.  */
    fnstenv (%rcx)
    fldenv  (%rcx)
    stmxcsr oMXCSR(%rdi)

    /* Save the current signal mask with
       rt_sigprocmask (SIG_BLOCK, NULL, set,_NSIG/8).  */
    leaq    oSIGMASK(%rdi), %rdx
    xorl    %esi,%esi
#if SIG_BLOCK == 0
    xorl    %edi, %edi
#else
    movl    $SIG_BLOCK, %edi
#endif
    movl    $_NSIG8,%r10d
    movl    $__NR_rt_sigprocmask, %eax
    syscall
    cmpq    $-4095, %rax        /* Check %rax for error.  */
    jae SYSCALL_ERROR_LABEL /* Jump to error handler if error.  */

    /* All done, return 0 for success.  */
    xorl    %eax, %eax
    ret
PSEUDO_END(__getcontext)

weak_alias (__getcontext, getcontext)


 void
  __makecontext (ucontext_t *ucp, void (*func) (void), int argc, ...)
  {
    extern void __start_context (void);
    greg_t *sp;
    unsigned int idx_uc_link;
    va_list ap; 
    int i;
  
    /* Generate room on stack for parameter if needed and uc_link.  */
    sp = (greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp
             + ucp->uc_stack.ss_size);
    sp -= (argc > 6 ? argc - 6 : 0) + 1;
    /* Align stack and make space for trampoline address.  */
    sp = (greg_t *) ((((uintptr_t) sp) & -16L) - 8); 
  
    idx_uc_link = (argc > 6 ? argc - 6 : 0) + 1;
  
    /* Setup context ucp.  */
    /* Address to jump to.  */
    ucp->uc_mcontext.gregs[REG_RIP] = (uintptr_t) func;
    /* Setup rbx.*/
    ucp->uc_mcontext.gregs[REG_RBX] = (uintptr_t) &sp[idx_uc_link];
    ucp->uc_mcontext.gregs[REG_RSP] = (uintptr_t) sp; 
  
    /* Setup stack.  */
    sp[0] = (uintptr_t) &__start_context;
    sp[idx_uc_link] = (uintptr_t) ucp->uc_link;
  
    va_start (ap, argc);
    /* Handle arguments.
  
       The standard says the parameters must all be int values.  This is
       an historic accident and would be done differently today.  For
       x86-64 all integer values are passed as 64-bit values and
       therefore extending the API to copy 64-bit values instead of
       32-bit ints makes sense.  It does not break existing
       functionality and it does not violate the standard which says
       that passing non-int values means undefined behavior.  */
    for (i = 0; i < argc; ++i)
      switch (i)
        {
        case 0:
      ucp->uc_mcontext.gregs[REG_RDI] = va_arg (ap, greg_t);
      break;
        case 1:
      ucp->uc_mcontext.gregs[REG_RSI] = va_arg (ap, greg_t);
      break;
        case 2:
      ucp->uc_mcontext.gregs[REG_RDX] = va_arg (ap, greg_t);
      break;
        case 3:
      ucp->uc_mcontext.gregs[REG_RCX] = va_arg (ap, greg_t);
      break;
        case 4:
      ucp->uc_mcontext.gregs[REG_R8] = va_arg (ap, greg_t);
      break;
        case 5:
      ucp->uc_mcontext.gregs[REG_R9] = va_arg (ap, greg_t);
      break;
        default:
      /* Put value on stack.  */
      sp[i - 5] = va_arg (ap, greg_t);
      break;
        }
    va_end (ap);
  }


ENTRY(__start_context)
    /* This removes the parameters passed to the function given to
       'makecontext' from the stack.  RBX contains the address
       on the stack pointer for the next context.  */
    movq    %rbx, %rsp

    /* Don't use pop here so that stack is aligned to 16 bytes.  */
    movq    (%rsp), %rdi        /* This is the next context.  */
    testq   %rdi, %rdi
    je  2f          /* If it is zero exit.  */

    call    __setcontext
    /* If this returns (which can happen if the syscall fails) we'll
       exit the program with the return error value (-1).  */
    movq    %rax,%rdi

2:
    call    HIDDEN_JUMPTARGET(exit)
    /* The 'exit' call should never return.  In case it does cause
       the process to terminate.  */
    hlt 
END(__start_context)
