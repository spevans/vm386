/* test_mod.c -- The test module.
   Simon Evans. */

#include <vmm/kernel.h>


struct test_module {
    struct module base;
};

const int some_data = 23;
static char some_bss[99];

extern struct mod_code_hdr *_test_start;
#define printf kernel->printf

bool test_init(void);

struct test_module test_module =
{
    MODULE_INIT("test", SYS_VER, test_init, NULL, NULL, NULL),
};



bool test_init(void)
{
    printf("test_init():\ntest_start\t%p\n", _test_start);
    printf("test_module\t%p\n", _test_start->mod_ptr);
    printf("test_end\t%p\n", _test_start->next_mod);
    printf("kernel_ptr\t%p\n",  _test_start->kernel_ptr);
    printf("kernel\t%p\n", kernel);
    printf("test_module\t%p\n", &test_module);
    printf("name\t%s\n", test_module.base.name);
    printf("mod_start\t%p\n", test_module.base.mod_start);
    printf("some_data = %d\n", some_data);
    printf("some_bss = %s\n", some_bss);
    return TRUE;
}

