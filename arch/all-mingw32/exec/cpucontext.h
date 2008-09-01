/*
    Copyright � 1995-2008, The AROS Development Team. All rights reserved.
    $Id$
*/

#ifndef _CPUCONTEXT_H
#define _CPUCONTEXT_H

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <errno.h>
#include "etask.h"
#include "winapi.h"

/* Put a value of type SP_TYPE on the stack or get it off the stack. */
#define _PUSH(sp,val)       (*--sp = (SP_TYPE)(val))
#define _POP(sp)            (*sp++)

#define SP_TYPE		IPTR
#define CPU_NUMREGS	0

#define SC_DISABLE(sc)   ((sc)->uc_sigmask = sig_int_mask)
#define SC_ENABLE(sc)    (sigemptyset(&(sc)->uc_sigmask))

/* this is from mingw32's w32api/winnt.h */
#ifdef __i386__
#define R0(context)     ((context)->Eax)
#define R1(context)     ((context)->Ebx)
#define R2(context)     ((context)->Ecx)
#define R3(context)     ((context)->Edx)
#define R4(context)     ((context)->Edi)
#define R5(context)     ((context)->Esi)
#define R6(context)     ((context)->EFlags)

#define FP(context)     ((context)->Ebp)
#define PC(context)     ((context)->Eip)
#define SP(context)     ((context)->Esp)

#define FPSTATE(context) ((context)->FloatSave)
#else
#error Unsupported CPU
#endif

/* Here we have to setup a complete stack frame
 so Exec_Exception thinks it was called as a
 normal function. */
#define SETUP_EXCEPTION(sc,arg)\
do                                        \
{                                         \
_PUSH(GetSP(SysBase->ThisTask), arg); \
_PUSH(GetSP(SysBase->ThisTask), arg); \
} while (0)

#define GetCpuContext(task)	((CONTEXT *)(GetIntETask(task)->iet_Context))
#define GetSP(task)		(*(SP_TYPE **)(&task->tc_SPReg))

#define PREPARE_INITIAL_FRAME(sp,startpc)          \
    do {                                           \
        FP(GetCpuContext(task)) = 0;               \
        PC(GetCpuContext(task)) = (IPTR)(startpc); \
    } while (0)

#define PREPARE_INITIAL_CONTEXT(task,startpc)      \
    do {                                           \
    } while (0)

#define SAVEREGS(task,th)                          \
    do {                                           \
        CONTEXT *cc = GetCpuContext(task);         \
        GetThreadContext(th, cc);                  \
        GetSP(task) = (SP_TYPE *)SP(cc);           \
    } while (0)

#define RESTOREREGS(task,th)                       \
    do {                                           \
        CONTEXT *cc = GetCpuContext(task);         \
        SP(cc) = (IPTR)GetSP(task);                \
	SetThreadContext(th, cc);                  \
    } while (0)

#define PRINT_SC(sc) \
	kprintf ("    SP=%08lx  FP=%08lx  PC=%08lx\n" \
		"    R0=%08lx  R1=%08lx  R2=%08lx  R3=%08lx\n" \
		"    R4=%08lx  R5=%08lx  R6=%08lx\n" \
	    , SP(sc), FP(sc), PC(sc) \
	    , R0(sc), R1(sc), R2(sc), R3(sc) \
	    , R4(sc), R5(sc), R6(sc) \
      );

#define PRINT_CPUCONTEXT(task) \
	kprintf ("    SP=%08lx  FP=%08lx  PC=%08lx\n" \
		"    R0=%08lx  R1=%08lx  R2=%08lx  R3=%08lx\n" \
		"    R4=%08lx  R5=%08lx  R6=%08lx\n" \
	    , (ULONG)(GetSP(task)) \
	    , FP(GetCpuContext(task)) \
	    , PC(GetCpuContext(task)) \
	    , R0(GetCpuContext(task)) \
	    , R1(GetCpuContext(task)) \
	    , R2(GetCpuContext(task)) \
	    , R3(GetCpuContext(task)) \
	    , R4(GetCpuContext(task)) \
	    , R5(GetCpuContext(task)) \
	    , R6(GetCpuContext(task)) \
      );

#endif /* _CPUCONTEXT_H */
