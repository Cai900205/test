#ifndef _COMPAT_FS_
#define _COMPAT_FS_

#include_next <linux/fs.h>

#if defined(COMPAT_VMWARE)
static inline struct inode *file_inode(struct file *f)
{
        return f->f_dentry->d_inode;
}
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_FS_ */
