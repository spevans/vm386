/* debug.h -- Definition of the debug module.
   Simon Evans. */

#ifndef _VMM_DEBUG_H
#define _VMM_DEBUG_H

#include <vmm/module.h>

struct debug_module
{
    struct module base;

    unsigned char *(*ncode)(unsigned char *, unsigned char *,
			    unsigned int, bool, bool);
};    


/* Some function prototypes. */

#ifdef DEBUG_MODULE

extern unsigned char *ncode(unsigned char *, unsigned char *, unsigned int,
			    bool, bool);
extern bool add_debug_commands(void);
extern void remove_debug_commands(void);

#endif /* DEBUG_MODULE */
#endif /* _VMM_DEBUG_H */
