/* time.h -- Time-related definitions.
   John Harper. */

#ifndef _VMM_TIME_H
#define _VMM_TIME_H

#include <vmm/types.h>
#include <vmm/tasks.h>

struct time_bits {
    int year;
    int month;				/* 1... */
    const char *month_name;
    const char *month_abbrev;
    int day;				/* 1... */
    int day_of_week;			/* 0==Monday, ... */
    const char *dow_name;
    const char *dow_abbrev;
    int hour;
    int minute;
    int second;
};

extern __inline__ void __delay(int loops)
{
        __asm__(".align 2,0x90\n1:\tdecl %0\n\tjns 1b": :"a" (loops):"ax");
}

#ifdef KERNEL
extern volatile time_t date;
extern volatile u_long timer_ticks;
#endif


/* Timer stuff. */

struct timer_req {
    u_long wakeup_ticks;
    struct timer_req *next;
    union {
	struct semaphore sem;
	struct {
	    void (*func)(void *user_data);
	    void *user_data;
	} func;
    } action;
    char type;
};
#define TIMER_TASK 0
#define TIMER_FUNC 1

extern inline void
set_timer_sem(struct timer_req *req, u_long ticks)
{
    req->wakeup_ticks = ticks;
    set_sem_blocked(&req->action.sem);
    req->type = TIMER_TASK;
}

extern inline void
set_timer_func(struct timer_req *req, u_long ticks,
	       void (*func)(void *), void *user_data)
{
    req->wakeup_ticks = ticks;
    req->action.func.func = func;
    req->action.func.user_data = user_data;
    req->type = TIMER_FUNC;
}

extern inline void
set_timer_interval(struct timer_req *req, u_long ticks)
{
    req->wakeup_ticks = ticks;
}

/* To convert from 18.2Hz ticks to 1024Hz ticks multiply by this
   number. */
#define TICKS_18_TO_1024 56


/* Prototypes. */

#ifdef KERNEL

extern void update_cmos_date(void);
extern void add_timer(struct timer_req *req);
extern void remove_timer(struct timer_req *req);
extern void sleep_for_ticks(u_long ticks);
extern void sleep_for(time_t length);
extern u_long get_timer_ticks(void);
extern void udelay(u_long usecs);
extern void expand_time(time_t cal, struct time_bits *tm);
extern time_t current_time(void);
extern void init_time(void);

#endif /* KERNEL */
#endif /* _VMM_TIME_H */
