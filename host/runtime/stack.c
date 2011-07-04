/*  -*- linux-c -*-
 * Stack tracing functions
 * Copyright (C) 2005-2009 Red Hat Inc.
 * Copyright (C) 2005 Intel Corporation.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STACK_C_
#define _STACK_C_

/** @file stack.c
 * @brief Stack Tracing Functions
 */

/** @addtogroup stack Stack Tracing Functions
 *
 * @{
 */

#include "sym.c"
#include "regs.h"
#include "unwind.c"

#define MAXBACKTRACE 20

/* If uprobes isn't in the kernel, pull it in from the runtime. */
#if defined(CONFIG_UTRACE)      /* uprobes doesn't work without utrace */
#if defined(CONFIG_UPROBES) || defined(CONFIG_UPROBES_MODULE)
#include <linux/uprobes.h>
#else
#include "uprobes/uprobes.h"
#endif
#ifndef UPROBES_API_VERSION
#define UPROBES_API_VERSION 1
#endif
#else
struct uretprobe_instance;
#endif

#if defined(STAPCONF_KERNEL_STACKTRACE)
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#endif

static void _stp_stack_print_fallback(unsigned long, int, int);

#if defined (__x86_64__)
#include "stack-x86_64.c"
#elif defined (__ia64__)
#include "stack-ia64.c"
#elif  defined (__i386__)
#include "stack-i386.c"
#elif defined (__powerpc__)
#include "stack-ppc.c"
#elif defined (__arm__)
#include "stack-arm.c"
#elif defined (__s390__) || defined (__s390x__)
#include "stack-s390.c"
#else
#error "Unsupported architecture"
#endif

#if defined(STAPCONF_KERNEL_STACKTRACE)

struct print_stack_data
{
        int verbose;
        int max_level;
        int level;
};

static void print_stack_warning(void *data, char *msg)
{
}

static void
print_stack_warning_symbol(void *data, char *msg, unsigned long symbol)
{
}

static int print_stack_stack(void *data, char *name)
{
	return -1;
}

static void print_stack_address(void *data, unsigned long addr, int reliable)
{
	struct print_stack_data *sdata = data;
        if (sdata->level++ < sdata->max_level)
                _stp_func_print(addr, sdata->verbose, 0, NULL);
}

static const struct stacktrace_ops print_stack_ops = {
	.warning = print_stack_warning,
	.warning_symbol = print_stack_warning_symbol,
	.stack = print_stack_stack,
	.address = print_stack_address,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
	.walk_stack = print_context_stack,
#endif
};

static void _stp_stack_print_fallback(unsigned long stack, int verbose, int levels)
{
        struct print_stack_data print_data;
        print_data.verbose = verbose;
        print_data.max_level = levels;
        print_data.level = 0;
        dump_trace(current, NULL, (long *)stack, 0, &print_stack_ops,
                   &print_data);
}
#endif

// Without KPROBES very little works atm.
// But this file is unconditionally imported, while these two functions are only
// used through context-unwind.stp.
#if defined (CONFIG_KPROBES)

/** Prints the stack backtrace
 * @param regs A pointer to the struct pt_regs.
 */

static void _stp_stack_print(struct pt_regs *regs, int verbose, struct kretprobe_instance *pi, int levels, struct task_struct *tsk, struct uretprobe_instance *ri)
{
	if (verbose) {
		/* print the current address */
		if (pi) {
			if (verbose == SYM_VERBOSE_FULL) {
				_stp_print("Returning from: ");
				_stp_symbol_print((unsigned long)_stp_probe_addr_r(pi));
				_stp_print("\nReturning to  : ");
			}
			_stp_symbol_print((unsigned long)_stp_ret_addr_r(pi));
			_stp_print_char('\n');
#ifdef CONFIG_UTRACE /* as a proxy for presence of uprobes... */
                } else if (ri && ri != GET_PC_URETPROBE_NONE) {
			if (verbose == SYM_VERBOSE_FULL) {
				_stp_print("Returning from: ");
				/* ... otherwise this dereference fails */
				_stp_usymbol_print(ri->rp->u.vaddr, tsk);
				_stp_print("\nReturning to  : ");
				_stp_usymbol_print(ri->ret_addr, tsk);
				_stp_print_char('\n');				
			} else
				_stp_func_print(ri->ret_addr, verbose, 0, tsk);
#endif
		} else if (verbose == SYM_VERBOSE_BRIEF) {
			_stp_func_print(REG_IP(regs), verbose, 0, tsk);
		} else {
			_stp_print_char(' ');
			if (tsk)
				_stp_usymbol_print(REG_IP(regs), tsk);
			else
				_stp_symbol_print(REG_IP(regs));
			_stp_print_char('\n');
		}
	} else if (pi)
		_stp_printf("%p %p ", (int64_t)(long)_stp_ret_addr_r(pi), (int64_t) REG_IP(regs));
	else 
		_stp_printf("%p ", (int64_t) REG_IP(regs));

	__stp_stack_print(regs, verbose, levels, tsk, ri);
}

/** Writes stack backtrace to a string
 *
 * @param str string
 * @param regs A pointer to the struct pt_regs.
 * @returns void
 */
static void _stp_stack_snprint(char *str, int size, struct pt_regs *regs, int verbose, struct kretprobe_instance *pi, int levels, struct task_struct *tsk, struct uretprobe_instance *ri)
{
	/* To get a string, we use a simple trick. First flush the print buffer, */
	/* then call _stp_stack_print, then copy the result into the output string  */
	/* and clear the print buffer. */
	_stp_pbuf *pb = per_cpu_ptr(Stp_pbuf, smp_processor_id());
	_stp_print_flush();
	_stp_stack_print(regs, verbose, pi, levels, tsk, ri);
	strlcpy(str, pb->buf, size < (int)pb->len ? size : (int)pb->len);
	pb->len = 0;
}

#endif /* CONFIG_KPROBES */

static void _stp_stack_print_tsk(struct task_struct *tsk, int verbose, int levels)
{
#if defined(STAPCONF_KERNEL_STACKTRACE)
        int i;
        unsigned long backtrace[MAXBACKTRACE];
        struct stack_trace trace;
        int maxLevels = min(levels, MAXBACKTRACE);
        memset(&trace, 0, sizeof(trace));
        trace.entries = &backtrace[0];
        trace.max_entries = maxLevels;
        trace.skip = 0;
        save_stack_trace_tsk(tsk, &trace);
        for (i = 0; i < maxLevels; ++i) {
                if (backtrace[i] == 0 || backtrace[i] == ULONG_MAX)
                        break;
		_stp_symbol_print(backtrace[i]);
		_stp_print_char('\n');
        }
#endif
}

/** Writes a task stack backtrace to a string
 *
 * @param str string
 * @param tsk A pointer to the task_struct
 * @returns void
 */
static void _stp_stack_snprint_tsk(char *str, int size, struct task_struct *tsk, int verbose, int levels)
{
	_stp_pbuf *pb = per_cpu_ptr(Stp_pbuf, smp_processor_id());
	_stp_print_flush();
	_stp_stack_print_tsk(tsk, verbose, levels);
	strlcpy(str, pb->buf, size < (int)pb->len ? size : (int)pb->len);
	pb->len = 0;
}
#endif /* _STACK_C_ */
