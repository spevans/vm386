#ifndef __IRQ_H__
#define __IRQ_H__

#include <vmm/types.h>

bool alloc_irq(u_int, void *, char *);
void dealloc_irq(u_int);
void init_irq_handlers(void);

extern void _irq0(void);
extern void _irq1(void);
extern void _irq2(void);
extern void _irq3(void);
extern void _irq4(void);
extern void _irq5(void);
extern void _irq6(void);
extern void _irq7(void);
extern void _irq8(void);
extern void _irq9(void);
extern void _irq10(void);
extern void _irq11(void);
extern void _irq12(void);
extern void _irq13(void);
extern void _irq14(void);
extern void _irq15(void);

extern u_long intr_nest_count;

/* Queued irqs. */
extern volatile u_int queued_irqs;
extern bool init_queued_irqs(void);
extern bool alloc_queued_irq(u_int irq, void (*func)(void), char *);
extern void dealloc_queued_irq(u_int irq);

struct shell;
extern void describe_irqs(struct shell *sh);

#endif
