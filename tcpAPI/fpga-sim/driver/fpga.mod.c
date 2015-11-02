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
	{ 0x9a31bb74, "module_layout" },
	{ 0xb7c59293, "cdev_del" },
	{ 0x5cd9dbb5, "kmalloc_caches" },
	{ 0xd2b09ce5, "__kmalloc" },
	{ 0xaa4fb4f8, "cdev_init" },
	{ 0x4c4fef19, "kernel_stack" },
	{ 0xadaabe1b, "pv_lock_ops" },
	{ 0x349cba85, "strchr" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x964bcb2, "boot_cpu_data" },
	{ 0x27e3ef6b, "device_destroy" },
	{ 0xb4e573a9, "filp_close" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x91715312, "sprintf" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0x3c80c06c, "kstrtoull" },
	{ 0x64ce4311, "current_task" },
	{ 0x27e1a049, "printk" },
	{ 0xe15f42bb, "_raw_spin_trylock" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x9166fada, "strncpy" },
	{ 0x5a921311, "strncmp" },
	{ 0xd4f29297, "device_create" },
	{ 0xebd1da60, "cdev_add" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x5fc1bf4e, "call_usermodehelper_fns" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xd61adcbd, "kmem_cache_alloc_trace" },
	{ 0xd52bf1ce, "_raw_spin_lock" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x4302d0eb, "free_pages" },
	{ 0x4f68e5c9, "do_gettimeofday" },
	{ 0x37a0cba, "kfree" },
	{ 0x1ac46550, "remap_pfn_range" },
	{ 0xf3248a6e, "class_destroy" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0x25827ecd, "__class_create" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xd4bb0c06, "filp_open" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v00001014d00003555sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "1608C997A535018C869A3C3");
