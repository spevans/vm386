#include <vmm/segment.h>
#include <vmm/kernel.h>

desc_table *get_gdt()
{
    return GDT;
}
