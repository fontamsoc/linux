// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/resume_user_mode.h>

#include <asm/exec.h>
#include <asm/thread_info.h>
#include <asm/ucontext.h>

struct sigframe {
	struct siginfo info;
	struct ucontext uc;
	pu32FaultReason faultreason; // Used to determine whether from a syscall.
	pu32_pt_regs_which which; // Determine which registers are valid.
};

static int restore_sigcontext (
	struct pu32_pt_regs *pu32regs,
	struct sigframe __user *frame) {
	struct pt_regs *regs = &frame->uc.uc_mcontext.regs;
	pu32regs->faultreason = frame->faultreason;
	switch ((pu32regs->which = frame->which)) {
		case PU32_PT_REGS_WHICH_SPFPRP:
			pu32regs->regs.sp = regs->sp;
			pu32regs->regs.fp = regs->fp;
			pu32regs->regs.rp = regs->rp;
			pu32regs->regs.pc = regs->pc;
			break;
		case PU32_PT_REGS_WHICH_ALL:
			return __copy_from_user (
				&pu32regs->regs, regs,
				sizeof(pu32regs->regs));
		default:
			return -EFAULT;
	}
	return 0;
}

SYSCALL_DEFINE0(rt_sigreturn) {
	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;
	// Always make any pending restarted system calls return -EINTR.
	tsk->restart_block.fn = do_no_restart_syscall;
	struct pu32_pt_regs *pu32regs = pu32_ti_pt_regs(ti);
	struct sigframe __user *frame = (struct sigframe __user *)pu32regs->regs.sp;
	if (!access_ok(frame, sizeof(*frame)))
		goto badframe;
	sigset_t set;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;
	set_current_blocked(&set);
	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;
	if (restore_sigcontext(pu32regs, frame))
		goto badframe;
	return pu32regs->regs.r1;
	badframe:;
	if (console_loglevel > 7) {
		pr_info_ratelimited (
			"%s: (%s:%d): badframe @ 0x%lx\n",
			__FUNCTION__, tsk->comm, task_pid_nr(tsk),
			(unsigned long)frame);
	}
	force_sig(SIGSEGV);
	return 0;
}

static int save_sigcontext (
	struct sigframe __user *frame,
	struct pu32_pt_regs *pu32regs) {
	frame->faultreason = pu32regs->faultreason;
	struct pt_regs *regs = &frame->uc.uc_mcontext.regs;
	switch ((frame->which = pu32regs->which)) {
		case PU32_PT_REGS_WHICH_SPFPRP:
			regs->sp = pu32regs->regs.sp;
			regs->fp = pu32regs->regs.fp;
			regs->rp = pu32regs->regs.rp;
			regs->pc = pu32regs->regs.pc;
			break;
		case PU32_PT_REGS_WHICH_ALL:
			return __copy_to_user (
				regs, &pu32regs->regs,
				sizeof(pu32regs->regs));
		default:
			return -EFAULT;
	}
	return 0;
}

static inline void __user *get_sigframe (
	struct ksignal *ksig,
	struct pt_regs *regs,
	size_t framesize) {
	unsigned long sp = regs->sp;
	if (on_sig_stack(sp) && !(on_sig_stack(sp - framesize)))
		return (void __user __force *)0;
	// This is the X/Open sanctioned signal stack switching.
	sp = sigsp(sp, ksig) - framesize;
	return (void __user *)arch_align_stack(sp);
}

static int setup_frame (
	struct ksignal *ksig,
	struct pu32_pt_regs *pu32regs) {

	struct pt_regs *regs = &pu32regs->regs;

	struct sigframe __user *frame =
		get_sigframe(ksig, regs, sizeof(*frame));

	if (!access_ok(frame, sizeof(*frame)))
		return -EFAULT;

	unsigned long err = 0;
	err |= copy_siginfo_to_user(&frame->info, &ksig->info);
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	sigset_t *set = sigmask_to_save();
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	err |= __save_altstack(&frame->uc.uc_stack, regs->sp);
	err |= save_sigcontext(frame, pu32regs);
	if (err)
		return -EFAULT;

	// Set up registers for signal handler.
	// Unmodified registers keep the value they had
	// from userspace at the time the signal was taken.
	// siginfo and ucontext is always passed regardless of SA_SIGINFO,
	// since some things rely on this (e.g. glibc's debug/segfault.c).
	extern void __tramp_rt_sigreturn(void);
	regs->rp = (unsigned long)__tramp_rt_sigreturn; // Setup to return from userspace.
	regs->pc = (unsigned long)ksig->ka.sa.sa_handler;
	regs->sp = (unsigned long)frame;
	regs->r1 = ksig->sig;                     // %1: signal number.
	regs->r2 = (unsigned long)(&frame->info); // %2: siginfo pointer.
	regs->r3 = (unsigned long)(&frame->uc);   // %3: ucontext pointer.
	pu32regs->faultreason = pu32PreemptIntr;

	if (console_loglevel > 7) {
		pr_info ("signalling(%s:%d) sig(%d) pc(0x%lx) rp(0x%lx) sp(0x%lx)\n",
			current->comm, task_pid_nr(current), ksig->sig,
			regs->pc, regs->rp, regs->sp);
	}

	return 0;
}

static void do_signal (void) {

	struct pu32_pt_regs *pu32regs = pu32_ti_pt_regs(current_thread_info());

	unsigned long in_syscall = (
		(pu32regs->faultreason == pu32SysOpIntr) &&
		(pu32regs->sysopcode & 0xff) == 0x01);

	struct pt_regs *regs = &pu32regs->regs;

	unsigned long r1 = regs->r1;

	struct ksignal ksig;
	if (get_signal(&ksig)) {
		// Deliver the signal.
		if (in_syscall) {
			switch (r1) {
				case -ERESTART_RESTARTBLOCK:
				case -ERESTARTNOHAND:
					regs->r1 = -EINTR;
					break;
				case -ERESTARTSYS:
					if (!(ksig.ka.sa.sa_flags & SA_RESTART)) {
						regs->r1 = -EINTR;
						break;
					}
					fallthrough;
				case -ERESTARTNOINTR:
					// regs->sr has syscall number to restart.
					// regs->pc is at the syscall instruction.
					pu32regs->faultreason = pu32PreemptIntr;
					break;
			}
		}
		signal_setup_done (
			setup_frame (&ksig, pu32regs),
			&ksig, 0);
		return;
	}

	if (in_syscall) {
		// Restart the system call; no handlers present.
		switch (r1) {
			case -ERESTART_RESTARTBLOCK:
			case -ERESTARTNOHAND:
			case -ERESTARTSYS:
			case -ERESTARTNOINTR:
				// regs->sr has syscall number to restart.
				// regs->pc is at the syscall instruction.
				pu32regs->faultreason = pu32PreemptIntr;
				break;
		}
	}

	// Restore saved sigmask if there is no signal to deliver.
	restore_saved_sigmask();
}

void do_notify_resume (
	unsigned long thread_info_flags) {

	if (thread_info_flags & (_TIF_SIGPENDING | _TIF_NOTIFY_SIGNAL))
		do_signal();

	if (thread_info_flags&_TIF_NOTIFY_RESUME) {
		resume_user_mode_work(
			&pu32_ti_pt_regs(current_thread_info())->regs);
	}
}
