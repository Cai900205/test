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
	{ 0xa71035d3, __VMLINUX_SYMBOL_STR(.pci_unregister_driver) },
	{ 0x196520d1, __VMLINUX_SYMBOL_STR(.cdev_del) },
	{ 0xc838631e, __VMLINUX_SYMBOL_STR(.__free_pages) },
	{ 0x3106dbe4, __VMLINUX_SYMBOL_STR(.__pci_register_driver) },
	{ 0x2f287f0d, __VMLINUX_SYMBOL_STR(.copy_to_user) },
	{ 0xd6c963c, __VMLINUX_SYMBOL_STR(.copy_from_user) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(.unregister_chrdev_region) },
	{ 0x52bb7de3, __VMLINUX_SYMBOL_STR(.__alloc_pages_nodemask) },
	{ 0xe1e2472, __VMLINUX_SYMBOL_STR(.cdev_add) },
	{ 0x9c97a227, __VMLINUX_SYMBOL_STR(.cdev_init) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(.alloc_chrdev_region) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(.sprintf) },
	{ 0x4074f48, __VMLINUX_SYMBOL_STR(.ioremap) },
	{ 0xdcb764ad, __VMLINUX_SYMBOL_STR(.memset) },
	{ 0xc2228414, __VMLINUX_SYMBOL_STR(.pci_request_regions) },
	{ 0xf437dcf3, __VMLINUX_SYMBOL_STR(.pci_set_master) },
	{ 0x1790fac2, __VMLINUX_SYMBOL_STR(.pci_enable_device) },
	{ 0x8b40ea2d, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0x50168f4d, __VMLINUX_SYMBOL_STR(contig_page_data) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(.printk) },
	{ 0xd6aca422, __VMLINUX_SYMBOL_STR(.dev_set_drvdata) },
	{ 0xfd5cb17c, __VMLINUX_SYMBOL_STR(.pci_release_regions) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(.iounmap) },
	{ 0x8fd74deb, __VMLINUX_SYMBOL_STR(.pci_domain_nr) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v00001957d00000440sv*sd*bc*sc*i*");
