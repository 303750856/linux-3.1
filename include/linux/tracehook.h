/*
 * Tracing hooks
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * This file defines hook entry points called by core code where
 * user tracing/debugging support might need to do something.  These
 * entry points are called tracehook_*().  Each hook declared below
 * has a detailed kerneldoc comment giving the context (locking et
 * al) from which it is called, and the meaning of its return value.
 *
 * Each function here typically has only one call site, so it is ok
 * to have some nontrivial tracehook_*() inlines.  In all cases, the
 * fast path when no tracing is enabled should be very short.
 *
 * The purpose of this file and the tracehook_* layer is to consolidate
 * the interface that the kernel core and arch code uses to enable any
 * user debugging or tracing facility (such as ptrace).  The interfaces
 * here are carefully documented so that maintainers of core and arch
 * code do not need to think about the implementation details of the
 * tracing facilities.  Likewise, maintainers of the tracing code do not
 * need to understand all the calling core or arch code in detail, just
 * documented circumstances of each call, such as locking conditions.
 *
 * If the calling core code changes so that locking is different, then
 * it is ok to change the interface documented here.  The maintainer of
 * core code changing should notify the maintainers of the tracing code
 * that they need to work out the change.
 *
 * Some tracehook_*() inlines take arguments that the current tracing
 * implementations might not necessarily use.  These function signatures
 * are chosen to pass in all the information that is on hand in the
 * caller and might conceivably be relevant to a tracer, so that the
 * core code won't have to be updated when tracing adds more features.
 * If a call site changes so that some of those parameters are no longer
 * already on hand without extra work, then the tracehook_* interface
 * can change so there is no make-work burden on the core code.  The
 * maintainer of core code changing should notify the maintainers of the
 * tracing code that they need to work out the change.
 */

#ifndef _LINUX_TRACEHOOK_H
#define _LINUX_TRACEHOOK_H	1

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/security.h>
#include <linux/utrace.h>
struct linux_binprm;

/*
 * ptrace report for syscall entry and exit looks identical.
 */
static inline void ptrace_report_syscall(struct pt_regs *regs)
{
	int ptrace = current->ptrace;

	if (!(ptrace & PT_SYSCALL_TRACE)) {
#ifdef TIF_SYSCALL_EMU
		if (!test_thread_flag(TIF_SYSCALL_EMU))
#endif
			return;
	}

	ptrace_notify(SIGTRAP | ((ptrace & PT_TRACESYSGOOD) ? 0x80 : 0));

	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}

/**
 * tracehook_report_syscall_entry - task is about to attempt a system call
 * @regs:		user register state of current task
 *
 * This will be called if %TIF_SYSCALL_TRACE has been set, when the
 * current task has just entered the kernel for a system call.
 * Full user register state is available here.  Changing the values
 * in @regs can affect the system call number and arguments to be tried.
 * It is safe to block here, preventing the system call from beginning.
 *
 * Returns zero normally, or nonzero if the calling arch code should abort
 * the system call.  That must prevent normal entry so no system call is
 * made.  If @task ever returns to user mode after this, its register state
 * is unspecified, but should be something harmless like an %ENOSYS error
 * return.  It should preserve enough information so that syscall_rollback()
 * can work (see asm-generic/syscall.h).
 *
 * Called without locks, just after entering kernel mode.
 */
static inline __must_check int tracehook_report_syscall_entry(
	struct pt_regs *regs)
{
	if ((task_utrace_flags(current) & UTRACE_EVENT(SYSCALL_ENTRY)) &&
	    utrace_report_syscall_entry(regs))
		return 1;
	ptrace_report_syscall(regs);
	return 0;
}

#define ptrace_wants_step(task)	\
	((task)->ptrace & (PT_SINGLE_STEP | PT_SINGLE_BLOCK))

/**
 * tracehook_report_syscall_exit - task has just finished a system call
 * @regs:		user register state of current task
 * @step:		nonzero if simulating single-step or block-step
 *
 * This will be called if %TIF_SYSCALL_TRACE has been set, when the
 * current task has just finished an attempted system call.  Full
 * user register state is available here.  It is safe to block here,
 * preventing signals from being processed.
 *
 * If @step is nonzero, this report is also in lieu of the normal
 * trap that would follow the system call instruction because
 * user_enable_block_step() or user_enable_single_step() was used.
 * In this case, %TIF_SYSCALL_TRACE might not be set.
 *
 * Called without locks, just before checking for pending signals.
 */
static inline void tracehook_report_syscall_exit(struct pt_regs *regs, int step)
{
	if (task_utrace_flags(current) & UTRACE_EVENT(SYSCALL_EXIT))
		utrace_report_syscall_exit(regs);

	if (step && ptrace_wants_step(current)) {
		siginfo_t info;
		user_single_step_siginfo(current, regs, &info);
		force_sig_info(SIGTRAP, &info, current);
		return;
	}

	ptrace_report_syscall(regs);
}

/**
 * tracehook_signal_handler - signal handler setup is complete
 * @sig:		number of signal being delivered
 * @info:		siginfo_t of signal being delivered
 * @ka:			sigaction setting that chose the handler
 * @regs:		user register state
 * @stepping:		nonzero if debugger single-step or block-step in use
 *
 * Called by the arch code after a signal handler has been set up.
 * Register and stack state reflects the user handler about to run.
 * Signal mask changes have already been made.
 *
 * Called without locks, shortly before returning to user mode
 * (or handling more signals).
 */
static inline void tracehook_signal_handler(int sig, siginfo_t *info,
					    const struct k_sigaction *ka,
					    struct pt_regs *regs, int stepping)
{
	if (task_utrace_flags(current))
		utrace_signal_handler(current, stepping);
	if (stepping && ptrace_wants_step(current))
		ptrace_notify(SIGTRAP);
}

/**
 * tracehook_consider_fatal_signal - suppress special handling of fatal signal
 * @task:		task receiving the signal
 * @sig:		signal number being sent
 *
 * Return nonzero to prevent special handling of this termination signal.
 * Normally handler for signal is %SIG_DFL.  It can be %SIG_IGN if @sig is
 * ignored, in which case force_sig() is about to reset it to %SIG_DFL.
 * When this returns zero, this signal might cause a quick termination
 * that does not give the debugger a chance to intercept the signal.
 *
 * Called with or without @task->sighand->siglock held.
 */
static inline int tracehook_consider_fatal_signal(struct task_struct *task,
						  int sig)
{
	if (unlikely(task_utrace_flags(task) & (UTRACE_EVENT(SIGNAL_TERM) |
						UTRACE_EVENT(SIGNAL_CORE))))
		return 1;
	return task->ptrace != 0;
}

#ifdef TIF_NOTIFY_RESUME
/**
 * set_notify_resume - cause tracehook_notify_resume() to be called
 * @task:		task that will call tracehook_notify_resume()
 *
 * Calling this arranges that @task will call tracehook_notify_resume()
 * before returning to user mode.  If it's already running in user mode,
 * it will enter the kernel and call tracehook_notify_resume() soon.
 * If it's blocked, it will not be woken.
 */
static inline void set_notify_resume(struct task_struct *task)
{
	if (!test_and_set_tsk_thread_flag(task, TIF_NOTIFY_RESUME))
		kick_process(task);
}

/**
 * tracehook_notify_resume - report when about to return to user mode
 * @regs:		user-mode registers of @current task
 *
 * This is called when %TIF_NOTIFY_RESUME has been set.  Now we are
 * about to return to user mode, and the user state in @regs can be
 * inspected or adjusted.  The caller in arch code has cleared
 * %TIF_NOTIFY_RESUME before the call.  If the flag gets set again
 * asynchronously, this will be called again before we return to
 * user mode.
 *
 * Called without locks.  However, on some machines this may be
 * called with interrupts disabled.
 */
static inline void tracehook_notify_resume(struct pt_regs *regs)
{
	struct task_struct *task = current;
	/*
	 * Prevent the following store/load from getting ahead of the
	 * caller which clears TIF_NOTIFY_RESUME. This pairs with the
	 * implicit mb() before setting TIF_NOTIFY_RESUME in
	 * set_notify_resume().
	 */
	smp_mb();
	if (task_utrace_flags(task))
		utrace_resume(task, regs);
}
#endif	/* TIF_NOTIFY_RESUME */

#endif	/* <linux/tracehook.h> */
