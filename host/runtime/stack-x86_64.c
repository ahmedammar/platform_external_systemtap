/* -*- linux-c -*-
 * x86_64 stack tracing functions
 * Copyright (C) 2005-2008 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

/* DWARF unwinder failed.  Just dump intereting addresses on kernel stack. */

#if !defined(STAPCONF_KERNEL_STACKTRACE)
static void _stp_stack_print_fallback(unsigned long stack, int verbose, int levels)
{
	unsigned long addr;
	while (levels && stack & (THREAD_SIZE - 1)) {
		if (unlikely(_stp_read_address(addr, (unsigned long *)stack, KERNEL_DS))) {
			/* cannot access stack.  give up. */
			return;
		}
		if (_stp_func_print(addr, verbose, 0, NULL))
			levels--;
		stack++;
	}
}
#endif


static void __stp_stack_print(struct pt_regs *regs, int verbose, int levels,
                              struct task_struct *tsk, struct uretprobe_instance *ri)
{
#ifdef STP_USE_DWARF_UNWINDER
        int start_levels = levels;
	// FIXME: large stack allocation
	struct unwind_frame_info info;
	arch_unw_init_frame_info(&info, regs);

	while (levels && (tsk || !arch_unw_user_mode(&info))) {
		int ret = unwind(&info, tsk);
#ifdef CONFIG_UTRACE
                unsigned long maybe_pc = 0;
                if (ri) {
                        maybe_pc = uprobe_get_pc(ri, UNW_PC(&info),
                                                 UNW_SP(&info));
                        if (!maybe_pc)
                                printk("SYSTEMTAP ERROR: uprobe_get_return returned 0\n");
                        else
                                UNW_PC(&info) = maybe_pc;
                }
#endif
		dbug_unwind(1, "ret=%d PC=%lx SP=%lx\n", ret, UNW_PC(&info), UNW_SP(&info));
		if (ret == 0) {
			_stp_func_print(UNW_PC(&info), verbose, 1, tsk);
			levels--;
			if (UNW_PC(&info) != _stp_kretprobe_trampoline)
			  continue;
		}
		/* If an error happened or we hit a kretprobe trampoline,
		 * use fallback backtrace, unless user task backtrace.
		 * FIXME: is there a way to unwind across kretprobe
		 * trampolines? PR9999. */
		if ((ret < 0
		     || UNW_PC(&info) == _stp_kretprobe_trampoline)
		    && ! (tsk || arch_unw_user_mode(&info)))
			_stp_stack_print_fallback(UNW_SP(&info), verbose, levels);
		return;
	}
#else /* ! STP_USE_DWARF_UNWINDER */
	_stp_stack_print_fallback(REG_SP(regs), verbose, levels);
#endif
}


