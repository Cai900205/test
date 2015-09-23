#ifndef _COMPAT_PROC_FS_
#define _COMPAT_PROC_FS_

#include_next <linux/proc_fs.h>

#if defined(COMPAT_VMWARE)
#define PDE_DATA(inode) (PDE(inode)->data)

struct proc_file_ops {
	read_proc_t *read;
	write_proc_t *write;
};

static inline struct proc_dir_entry *proc_mkdir_data(const char *name,
	umode_t mode, struct proc_dir_entry *parent, void *data)
{
	struct proc_dir_entry *pde = proc_mkdir(name, parent);
	if (pde) {
		pde->data = data;
		pde->mode &= ~S_IALLUGO;
		pde->mode |= mode & S_IALLUGO;
	}
	return pde;
}

static inline struct proc_dir_entry *proc_create_data(const char *name,
	umode_t mode, struct proc_dir_entry * parent,
	const struct proc_file_ops * fops, void *data)
{
	struct proc_dir_entry *pde = create_proc_entry(name, mode, parent);
	if (pde){
		pde->data = data;
		if (fops){
			pde->read_proc = fops->read;
			pde->write_proc = fops->write;
		}
	}
	return pde;
}

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_PROC_FS_ */
