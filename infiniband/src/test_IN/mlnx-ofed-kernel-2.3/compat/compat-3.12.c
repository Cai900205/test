#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/export.h>

#define debugfs_create_atomic_t LINUX_BACKPORT(debugfs_create_atomic_t)

static int debugfs_atomic_t_set(void *data, u64 val)
{
	atomic_set((atomic_t *)data, val);
	return 0;
}
static int debugfs_atomic_t_get(void *data, u64 *val)
{
	*val = atomic_read((atomic_t *)data);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_atomic_t, debugfs_atomic_t_get,
			debugfs_atomic_t_set, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_atomic_t_ro, debugfs_atomic_t_get, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_atomic_t_wo, NULL, debugfs_atomic_t_set, "%lld\n");

/**
 * debugfs_create_atomic_t - create a debugfs file that is used to read and
 * write an atomic_t value
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have
 * @parent: a pointer to the parent dentry for this file.  This should be a
 *          directory dentry if set.  If this parameter is %NULL, then the
 *          file will be created in the root of the debugfs filesystem.
 * @value: a pointer to the variable that the file should read to and write
 *         from.
 */
struct dentry *debugfs_create_atomic_t(const char *name, umode_t mode,
				 struct dentry *parent, atomic_t *value)
{
	/* if there are no write bits set, make read only */
	if (!(mode & S_IWUGO))
		return debugfs_create_file(name, mode, parent, value,
					&fops_atomic_t_ro);
	/* if there are no read bits set, make write only */
	if (!(mode & S_IRUGO))
		return debugfs_create_file(name, mode, parent, value,
					&fops_atomic_t_wo);

	return debugfs_create_file(name, mode, parent, value, &fops_atomic_t);
}
EXPORT_SYMBOL_GPL(debugfs_create_atomic_t);
