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
	{ 0x14522340, "module_layout" },
	{ 0x42e80c19, "cdev_del" },
	{ 0x4f1939c7, "per_cpu__current_task" },
	{ 0x5a34a45c, "__kmalloc" },
	{ 0x7ee91c1d, "_spin_trylock" },
	{ 0xc45a9f63, "cdev_init" },
	{ 0x4d71232a, "call_usermodehelper_setfns" },
	{ 0xcde6aeae, "call_usermodehelper_exec" },
	{ 0x349cba85, "strchr" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0xd691cba2, "malloc_sizes" },
	{ 0xdd822018, "boot_cpu_data" },
	{ 0x973873ab, "_spin_lock" },
	{ 0x7edc1537, "device_destroy" },
	{ 0x6f0154d3, "filp_close" },
	{ 0xebd273a6, "strict_strtoull" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0xea147363, "printk" },
	{ 0x23269a13, "strict_strtoul" },
	{ 0x7ec9bfbc, "strncpy" },
	{ 0x85f8a266, "copy_to_user" },
	{ 0xb4390f9a, "mcount" },
	{ 0x85abc85f, "strncmp" },
	{ 0x6dcaeb88, "per_cpu__kernel_stack" },
	{ 0x2d2cf7d, "device_create" },
	{ 0xa6d1bdca, "cdev_add" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x2044fa9e, "kmem_cache_alloc_trace" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xdeac3097, "call_usermodehelper_setup" },
	{ 0x1d2e87c6, "do_gettimeofday" },
	{ 0x37a0cba, "kfree" },
	{ 0xc911f7f0, "remap_pfn_range" },
	{ 0xe06bb002, "class_destroy" },
	{ 0xa2654165, "__class_create" },
	{ 0x3302b500, "copy_from_user" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x371af43, "filp_open" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v00001014d00003555sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "1608C997A535018C869A3C3");

static const struct rheldata _rheldata __used
__attribute__((section(".rheldata"))) = {
	.rhel_major = 6,
	.rhel_minor = 6,
};
