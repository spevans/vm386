/* test_mod.c -- The test module.
   Simon Evans. */

#include <vmm/kernel.h>

struct kernel_module *kernel;

struct test_module {
    struct module base;
};

extern bool test_init(void);

struct test_module test_module =
{
    MODULE_INIT("test", SYS_VER, test_init, NULL, NULL, NULL),
};
