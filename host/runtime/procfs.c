/* -*- linux-c -*-
 *
 * /proc command channels
 * Copyright (C) 2007-2009 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

/* The maximum number of files AND directories that can be opened.
 * It would be great if the translator would emit this based on the actual
 * number of needed files.
 */

#ifndef _STP_PROCFS_C_
#define _STP_PROCFS_C_

#ifndef STP_MAX_PROCFS_FILES
#define STP_MAX_PROCFS_FILES 16
#endif

static int _stp_num_pde = 0;
static struct proc_dir_entry *_stp_pde[STP_MAX_PROCFS_FILES];
static struct proc_dir_entry *_stp_procfs_files[STP_MAX_PROCFS_FILES];
static struct proc_dir_entry *_stp_proc_stap = NULL;
static struct proc_dir_entry *_stp_proc_root = NULL;

static void _stp_close_procfs(void);

// 2.6.24 fixed proc_dir_entry refcounting.
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define LAST_ENTRY_COUNT 0
#else
#define LAST_ENTRY_COUNT 1
#endif

/*
 * Removes /proc/systemtap/{module_name} and /proc/systemtap (if empty)
 */
static void _stp_rmdir_proc_module(void)
{
	if (!_stp_lock_transport_dir()) {
		errk("Unable to lock transport directory.\n");
		return;
	}

        if (_stp_proc_root && _stp_proc_root->subdir == NULL) {
		if (atomic_read(&_stp_proc_root->count) != LAST_ENTRY_COUNT)
			_stp_warn("Removal of /proc/systemtap/%s\nis deferred until it is no longer in use.\n"
				  "Systemtap module removal will block.\n", THIS_MODULE->name);	
		remove_proc_entry(THIS_MODULE->name, _stp_proc_stap);
		_stp_proc_root = NULL;
	}

	if (_stp_proc_stap && _stp_proc_stap->subdir == NULL) {
		/* Important! Do not attempt removal of /proc/systemtap */
		/* if in use.  This will put the PDE in deleted state */
		/* pending usage count dropping to 0. During this time, */
		/* path_lookup() will still find it and allow new */
		/* modules to use it, even though it will not show up */
		/* in directory listings. */

 		if (atomic_read(&_stp_proc_stap->count) == 0) {
			remove_proc_entry("systemtap", NULL);
			_stp_proc_stap = NULL;
		}
	}
	_stp_unlock_transport_dir();
}


/*
 * Safely creates /proc/systemtap (if necessary) and
 * /proc/systemtap/{module_name}.
 */
static int _stp_mkdir_proc_module(void)
{	
        if (_stp_proc_root == NULL) {
		struct nameidata nd;

		if (!_stp_lock_transport_dir()) {
			errk("Unable to lock transport directory.\n");
			goto done;
		}
		
		/* We use path_lookup() because there is no lookup */
		/* function for procfs we can call directly.  And */
		/* proc_mkdir() will always succeed, creating multiple */
		/* directory entries, all with the same name. */

		if (path_lookup("/proc/systemtap", 0, &nd)) {
			/* doesn't exist, so create it */
			_stp_proc_stap = proc_mkdir ("systemtap", NULL);
			if (_stp_proc_stap == NULL) {
				_stp_unlock_transport_dir();
				goto done;
			}
		} else {
                        #ifdef STAPCONF_NAMEIDATA_CLEANUP
                        _stp_proc_stap = PDE(nd.path.dentry->d_inode);
                        path_put (&nd.path);

                        #else
			_stp_proc_stap = PDE(nd.dentry->d_inode);
			path_release (&nd);
			#endif
		}
		
		_stp_proc_root = proc_mkdir(THIS_MODULE->name, _stp_proc_stap);
#ifdef AUTOCONF_PROCFS_OWNER
		if (_stp_proc_root != NULL)
			_stp_proc_root->owner = THIS_MODULE;
#endif

		_stp_unlock_transport_dir();
	}
done:
	return (_stp_proc_root) ? 1 : 0;
}

/*
 * This checks our local cache to see if we already made the dir.
 */
static struct proc_dir_entry *_stp_procfs_lookup(const char *dir, struct proc_dir_entry *parent)
{
	int i;
	for (i = 0; i <_stp_num_pde; i++) {
		struct proc_dir_entry *pde = _stp_pde[i];
		if (pde->parent == parent && !strcmp(dir, pde->name))
			return pde;
	}
	return NULL;
}

static int _stp_create_procfs(const char *path, int num,
			      const struct file_operations *fops)
{  
	const char *p;
	char *next;
	struct proc_dir_entry *last_dir, *de;

	if (num >= STP_MAX_PROCFS_FILES) {
		_stp_error("Requested file number %d is larger than max (%d)\n", 
			   num, STP_MAX_PROCFS_FILES);
		return -1;
	}

	_stp_mkdir_proc_module();
	last_dir = _stp_proc_root;

	/* if no path, use default one */
	if (strlen(path) == 0)
		p = "command";
	else
		p = path;
	
	while ((next = strchr(p, '/'))) {
		if (_stp_num_pde == STP_MAX_PROCFS_FILES)
			goto too_many;
		*next = 0;
		de = _stp_procfs_lookup(p, last_dir);
		if (de == NULL) {
			    last_dir = proc_mkdir(p, last_dir);
			    if (!last_dir) {
				    _stp_error("Could not create directory \"%s\"\n", p);
				    goto err;
			    }
			    _stp_pde[_stp_num_pde++] = last_dir;
#ifdef AUTOCONF_PROCFS_OWNER
			    last_dir->owner = THIS_MODULE;
#endif
			    last_dir->uid = _stp_uid;
			    last_dir->gid = _stp_gid;
		} else {
			if (!S_ISDIR(de->mode)) {
				_stp_error("Could not create directory \"%s\"\n", p);
				goto err;
			}
			last_dir = de;
		}
		p = next + 1;
	}
	
	if (_stp_num_pde == STP_MAX_PROCFS_FILES)
		goto too_many;
	
	de = proc_create(p, 0600, last_dir, fops);
	if (de == NULL) {
		_stp_error("Could not create file \"%s\" in path \"%s\"\n", p, path);
		goto err;
	}
#ifdef AUTOCONF_PROCFS_OWNER
	de->owner = THIS_MODULE;
#endif
	de->uid = _stp_uid;
	de->gid = _stp_gid;
	_stp_pde[_stp_num_pde++] = de;
	_stp_procfs_files[num] = de;
	return 0;
	
too_many:
	_stp_error("Attempted to open too many procfs files. Maximum is %d\n", STP_MAX_PROCFS_FILES);
err:
	_stp_close_procfs();
	return -1;
}

static void _stp_close_procfs(void)
{
	int i;
	for (i = _stp_num_pde-1; i >= 0; i--) {
		struct proc_dir_entry *pde = _stp_pde[i];
		remove_proc_entry(pde->name, pde->parent);
	}
	_stp_num_pde = 0;
	_stp_rmdir_proc_module();
}

#endif	/* _STP_PROCFS_C_ */
