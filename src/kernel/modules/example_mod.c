/* example_mod.c -- An example of how modules work
   John Harper  */

#include <vmm/module.h>
#include <vmm/kernel.h>

/* This would be in a header file. */
struct example_module {
    struct module base;
    /* exports */
    int (*example_function)(int, int);
    short example_var;
};

bool example_init(void);
struct module *example_open(void);
void example_close(struct module *mod);
bool example_expunge(void);
int example_function(int, int);

struct example_module example_module = {
    MODULE_INIT("example", SYS_VER, example_init, example_open,
		example_close, example_expunge),
    example_function,
    42
};

/* This variable is automagically initialised by the module loader. */
struct kernel_module *kernel;

bool
example_init(void)
{
    /* no initialisation needed. */
    return TRUE;
}

struct module *
example_open(void)
{
    example_module.base.open_count++;
    return &example_module.base;
}

void
example_close(struct module *mod)
{
    example_module.base.open_count--;
}

bool
example_expunge(void)
{
    if(example_module.base.open_count == 0)
	return TRUE;
    else
	return FALSE;
}

int
example_function(int x, int y)
{
    return x * example_module.example_var + y;
}
