/* -*- linux-c -*- 
 * Copyright (C) 2010 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _UPROBE_COMMON_H_
#define _UPROBE_COMMON_H_

struct stap_uprobe {
  union { struct uprobe up; struct uretprobe urp; };
  int spec_index; /* index into stap_uprobe_specs; <0 == free && unregistered */
  unsigned long sdt_sem_address;
};

struct stap_uprobe_tf {
  struct stap_task_finder_target finder;
  const char *pathname;
};

struct stap_uprobe_spec {
  unsigned tfi;
  unsigned return_p:1;
  unsigned long address;
  const char *pp;
  void (*ph) (struct context*);
  unsigned long sdt_sem_offset;
 };

static int stap_uprobe_process_found (struct stap_task_finder_target *tgt, struct task_struct *tsk, int register_p, int process_p);
static int stap_uprobe_mmap_found (struct stap_task_finder_target *tgt, struct task_struct *tsk, char *path, unsigned long addr, unsigned long length, unsigned long offset, unsigned long vm_flags);
static int stap_uprobe_munmap_found (struct stap_task_finder_target *tgt, struct task_struct *tsk, unsigned long addr, unsigned long length);
static int stap_uprobe_process_munmap (struct stap_task_finder_target *tgt, struct task_struct *tsk, int register_p, int process_p);

#endif /* _UPROBE_COMMON_H_ */
