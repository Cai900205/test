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
	{ 0xa75312bc, __VMLINUX_SYMBOL_STR(.call_rcu_sched) },
	{ 0x2d3385d3, __VMLINUX_SYMBOL_STR(system_wq) },
	{ 0x404066c5, __VMLINUX_SYMBOL_STR(.idr_for_each) },
	{ 0xa94a8330, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x8b40ea2d, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0xb6b46a7c, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(.snprintf) },
	{ 0xd6ee688f, __VMLINUX_SYMBOL_STR(.vmalloc) },
	{ 0xc8b57c27, __VMLINUX_SYMBOL_STR(autoremove_wake_function) },
	{ 0x6b7f1917, __VMLINUX_SYMBOL_STR(.idr_find_slowpath) },
	{ 0x4c1182cb, __VMLINUX_SYMBOL_STR(.bitmap_scnprintf) },
	{ 0xc6cbbc89, __VMLINUX_SYMBOL_STR(.capable) },
	{ 0x60a13e90, __VMLINUX_SYMBOL_STR(.rcu_barrier) },
	{ 0x1c474caf, __VMLINUX_SYMBOL_STR(.kthread_stop) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(.kfree) },
	{ 0x10e438c9, __VMLINUX_SYMBOL_STR(.put_page) },
	{ 0xa1c99385, __VMLINUX_SYMBOL_STR(.__init_waitqueue_head) },
	{ 0x8f8e53b9, __VMLINUX_SYMBOL_STR(.idr_alloc) },
	{ 0xffd5a395, __VMLINUX_SYMBOL_STR(default_wake_function) },
	{ 0x519b0da3, __VMLINUX_SYMBOL_STR(.finish_wait) },
	{ 0xbfbbb382, __VMLINUX_SYMBOL_STR(.remap_vmalloc_range) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(.__kmalloc) },
	{ 0x6e764332, __VMLINUX_SYMBOL_STR(.idr_destroy) },
	{ 0xde48e9ca, __VMLINUX_SYMBOL_STR(._raw_spin_lock) },
	{ 0xc6a9176f, __VMLINUX_SYMBOL_STR(.vmalloc_to_page) },
	{ 0xca22f5f7, __VMLINUX_SYMBOL_STR(.get_user_pages_fast) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(.schedule) },
	{ 0x5635a60a, __VMLINUX_SYMBOL_STR(.vmalloc_user) },
	{ 0xd50f9727, __VMLINUX_SYMBOL_STR(.set_cpus_allowed_ptr) },
	{ 0x801678, __VMLINUX_SYMBOL_STR(.flush_scheduled_work) },
	{ 0x84ffea8b, __VMLINUX_SYMBOL_STR(.idr_preload) },
	{ 0x9af496c6, __VMLINUX_SYMBOL_STR(.misc_register) },
	{ 0xd10c3452, __VMLINUX_SYMBOL_STR(.idr_init) },
	{ 0xd6c963c, __VMLINUX_SYMBOL_STR(.copy_from_user) },
	{ 0x4829a47e, __VMLINUX_SYMBOL_STR(.memcpy) },
	{ 0x7031b164, __VMLINUX_SYMBOL_STR(.wake_up_process) },
	{ 0x2f287f0d, __VMLINUX_SYMBOL_STR(.copy_to_user) },
	{ 0x2e0d2f7f, __VMLINUX_SYMBOL_STR(.queue_work_on) },
	{ 0x735d8503, __VMLINUX_SYMBOL_STR(.add_wait_queue) },
	{ 0x5e3a8a9c, __VMLINUX_SYMBOL_STR(.__wake_up) },
	{ 0xdcb764ad, __VMLINUX_SYMBOL_STR(.memset) },
	{ 0x5f28c2dc, __VMLINUX_SYMBOL_STR(.idr_remove) },
	{ 0xb4ae39f4, __VMLINUX_SYMBOL_STR(.kmem_cache_alloc) },
	{ 0x79aa04a2, __VMLINUX_SYMBOL_STR(.get_random_bytes) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(.printk) },
	{ 0xbff0a954, __VMLINUX_SYMBOL_STR(.misc_deregister) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(.kthread_should_stop) },
	{ 0x48404b9a, __VMLINUX_SYMBOL_STR(.remove_wait_queue) },
	{ 0x7787d41c, __VMLINUX_SYMBOL_STR(.kthread_create_on_node) },
	{ 0x999e8297, __VMLINUX_SYMBOL_STR(.vfree) },
	{ 0xcf55922, __VMLINUX_SYMBOL_STR(.set_page_dirty_lock) },
	{ 0x8d2e268b, __VMLINUX_SYMBOL_STR(param_ops_ulong) },
	{ 0x47c8baf4, __VMLINUX_SYMBOL_STR(param_ops_uint) },
	{ 0xaf2d872c, __VMLINUX_SYMBOL_STR(.prepare_to_wait) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "B7ED40A79D5858ED63548C0");
