/* test_mod.c -- The test module.
   Simon Evans. */

#include <vmm/kernel.h>


struct test_module {
    struct module base;
};

const int some_data = 23;
static char some_bss[99];
char *data_ptr = &some_bss[20];
int const *int_ptr = &some_data;

#define printf kernel->printf

bool test_init(void);
bool test_expunge(void);

struct test_module test_module =
{
    MODULE_INIT("test", SYS_VER, test_init, NULL, NULL, test_expunge),
};


bool
test_init(void)
{
    printf("kernel\t\t%8p\n", kernel);
    printf("test_module\t%8p\n", &test_module);
    printf("name ptr\t%8p\n", test_module.base.name);
    printf("name\t\t%s\n", test_module.base.name);
    printf("mod_memory\t%8p\n", test_module.base.mod_memory);
    printf("mod_start\t%8p\n", test_module.base.mod_memory->text);
    printf("some_data\t%8p, %d\n", &some_data,some_data);
    printf("some_bss\t%8p, `%s'\n", &some_bss, some_bss);
    struct module_memory *mem = test_module.base.mod_memory;
    printf("text = %p rodata = %p data = %p bss = %p\n", mem->text, mem->rodata,
           mem->data, mem->bss);
    return TRUE;
}


bool
test_expunge(void)
{
        printf("Expunging test\n");
        return TRUE;
}
