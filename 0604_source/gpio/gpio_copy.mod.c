#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xc9b3c189, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(.kfree) },
	{ 0x196520d1, __VMLINUX_SYMBOL_STR(.cdev_del) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(.iounmap) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(.unregister_chrdev_region) },
	{ 0xe1e2472, __VMLINUX_SYMBOL_STR(.cdev_add) },
	{ 0x9c97a227, __VMLINUX_SYMBOL_STR(.cdev_init) },
	{ 0xdcb764ad, __VMLINUX_SYMBOL_STR(.memset) },
	{ 0xb4ae39f4, __VMLINUX_SYMBOL_STR(.kmem_cache_alloc) },
	{ 0xd8e484f0, __VMLINUX_SYMBOL_STR(.register_chrdev_region) },
	{ 0x4074f48, __VMLINUX_SYMBOL_STR(.ioremap) },
	{ 0xa94a8330, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x2f287f0d, __VMLINUX_SYMBOL_STR(.copy_to_user) },
	{ 0xd6c963c, __VMLINUX_SYMBOL_STR(.copy_from_user) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(.printk) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

