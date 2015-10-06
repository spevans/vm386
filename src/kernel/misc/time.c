/* time.c -- Timer interrupt handler and misc time functions.
   John Harper/Simon Evans. */

#ifdef TEST_TIME
#define TEST
#endif

#ifdef TEST
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#define __NO_TYPE_CLASHES
#endif

#include <vmm/types.h>
#include <vmm/time.h>
#include <vmm/irq.h>
#include <vmm/kernel.h>
#include <vmm/page.h>
#include <vmm/mc146818rtc.h>
#define RTC_ALWAYS_BCD 1
#include <vmm/tasks.h>
#include <vmm/string.h>
#include <vmm/io.h>

volatile u_long timer_ticks;
volatile u_long mums;
static volatile time32_t date;
volatile unsigned long delay_loop_counter;


#ifndef TEST
static struct timer_req *timer_list;
static int cmos_time_update;

/* Timer IRQ handler. */

static void
timer_intr(void)
{
}

/* micro second delay */
void
udelay(u_long usecs)
{
    u_long count = mums;

    count /= 1000000;
    count *= usecs;
    __delay(count);
}

void
add_timer(struct timer_req *req)
{
    struct timer_req **x;
    req->wakeup_ticks += timer_ticks;
    cli();
    x = &timer_list;
    while(((*x) != NULL) && ((*x)->wakeup_ticks < req->wakeup_ticks))
	x = &(*x)->next;
    req->next = *x;
    *x = req;
    sti();
}

void
remove_timer(struct timer_req *req)
{
    struct timer_req **x;
    cli();
    x = &timer_list;
    while((*x) != NULL)
    {
	if(*x == req)
	{
	    *x = (*x)->next;
	    break;
	}
	x = &(*x)->next;
    }
    sti();
}

/* Sleep for TICKS 1024Hz ticks. */
void
sleep_for_ticks(u_long ticks)
{
    struct timer_req timer;
    set_timer_sem(&timer, ticks);
    add_timer(&timer);
    wait(&timer.action.sem);
}

/* Sleep for LENGTH seconds. */
void
sleep_for(time32_t length)
{
    sleep_for_ticks(length * 1024);
}


/* The following two functions were borrowed from Linux's kernel/time.c */

/* converts date to days since 1/1/1970
 * assumes year,mon,day in normal date format
 * ie. 1/1/1970 => year=1970, mon=1, day=1
 *
 * For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.
 *
 * This algorithm was first published by Gauss (I think).
 */
static inline time32_t
mktime(unsigned int year, unsigned int mon,
       unsigned int day, unsigned int hour,
       unsigned int min, unsigned int sec)
{
	if (0 >= (int) (mon -= 2)) {	/* 1..12 -> 11,12,1..10 */
		mon += 12;	/* Puts Feb last since it has leap day */
		year -= 1;
	}
	return ((((time32_t)(year/4 - year/100 + year/400 + 367*mon/12 + day) +
		  year*365 - 719499
		  )*24 + hour /* now have hours */
		 )*60 + min /* now have minutes */
		)*60 + sec; /* finally seconds */
}

void
update_cmos_date()
{
    int second, minute, hour, day, month, year;

    cmos_time_update = 1;
    second = CMOS_READ(RTC_SECONDS);
    minute = CMOS_READ(RTC_MINUTES);
    hour = CMOS_READ(RTC_HOURS);
    day = CMOS_READ(RTC_DAY_OF_MONTH);
    month = CMOS_READ(RTC_MONTH);
    year = CMOS_READ(RTC_YEAR);
    if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
        BCD_TO_BIN(second);
        BCD_TO_BIN(minute);
        BCD_TO_BIN(hour);
        BCD_TO_BIN(day);
        BCD_TO_BIN(month);
        BCD_TO_BIN(year);
    }
    if ((year += 1900) < 1970)
        year += 100;

    date = mktime(year, month, day, hour, minute, second);
    cmos_time_update = 0;
}

#define CTICK_ADDR TO_LOGICAL(0xb8000 + 630, u_char *)
static void
cmos_timer(void)
{
    static int got_date = 0;
 
    int status;
    status = CMOS_READ(0xc);
    if(status & 0x40) {
	if((++timer_ticks % 1024) == 0)
		date++;

	/* Dispatch any expired timers... */
	while((timer_list != NULL)
	      && (timer_list->wakeup_ticks <= timer_ticks))
	{
	    struct timer_req *req = timer_list;
	    timer_list = timer_list->next;
	    switch(req->type)
	    {
	    case TIMER_TASK:
		signal(&req->action.sem);
		break;

	    case TIMER_FUNC:
		req->action.func.func(req->action.func.user_data);
		break;
	    }
	}

	/* When the current task's quantum runs out cause schedule()
	   to be called... */
	if(current_task && (--current_task->time_left <= 0))
	    kernel_module.need_resched = need_resched = TRUE;
    }
    if(status & 0x10) {
	if(got_date == 1) return;
        update_cmos_date();
	got_date = 1;
    }
}

u_long
get_timer_ticks(void)
{
    return timer_ticks;
}

#if 0
static time32_t
get_cmos_date(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int i;

	/* checking for Update-In-Progress could be done more elegantly
	 * (using the "update finished"-interrupt for example), but that
	 * would require excessive testing. promise I'll do that when I find
	 * the time.			- Torsten
	 */
	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
		if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
			break;
	for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms*/
		if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
			break;
	do { /* Isn't this overkill ? UIP above should guarantee consistency */
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));
	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
	  {
	    BCD_TO_BIN(sec);
	    BCD_TO_BIN(min);
	    BCD_TO_BIN(hour);
	    BCD_TO_BIN(day);
	    BCD_TO_BIN(mon);
	    BCD_TO_BIN(year);
	  }
	if ((year += 1900) < 1970)
		year += 100;

	return mktime(year, mon, day, hour, min, sec);
}
#endif

#endif /* !TEST */


/* Time utility functions. */

time32_t
current_time(void)
{
#ifdef TEST
    return (time32_t)time(NULL);
#else
    while(cmos_time_update);
    return date;
#endif
}


/* Time output stuff. */

#define EPOCH_YEAR 1970
#define SECS_PER_MIN 60
#define MINS_PER_HOUR 60
#define HOURS_PER_DAY 24
#define MONTHS_PER_YEAR 12
#define DAYS_PER_WEEK 7
#define DAYS_PER_YEAR 365
#define DAYS_PER_LEAP_YEAR 366

static const int days_per_month[2][MONTHS_PER_YEAR] = {
    { 31, 28, 31, 31, 30, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 31, 30, 30, 31, 31, 30, 31, 30, 31 }
};

static const char *month_names[MONTHS_PER_YEAR] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *month_abbrevs[MONTHS_PER_YEAR] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *dow_names[DAYS_PER_WEEK] = {
    "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday"
};

static const char *dow_abbrevs[DAYS_PER_WEEK] = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

/* Returns 1 if YR is a leap year, 0 otherwise. This is used to index the
   array days_per_month! */
static inline int
leap_year(int yr)
{
    /* Is this correct? */
    return ((yr % 4) == 0) && (((yr % 100) != 0) || ((yr % 400) == 0));
}

void
expand_time(time32_t cal, struct time_bits *tm)
{
    int month, year;
    DB(("expand_time: cal=%Z tm=%p\n", cal, tm));
    tm->second = cal % SECS_PER_MIN;
    cal /= SECS_PER_MIN;
    tm->minute = cal % MINS_PER_HOUR;
    cal /= MINS_PER_HOUR;
    tm->hour = cal % HOURS_PER_DAY;
    cal /= HOURS_PER_DAY;

    /* There must be a better way to do this than all this
       looping.. */

    year = EPOCH_YEAR;
    while(1)
    {
	time32_t nxt = leap_year(year) ? DAYS_PER_LEAP_YEAR : DAYS_PER_YEAR;
	if(cal < nxt)
	    break;
	cal -= nxt;
	year++;
    }
    tm->year = year;

    month = 0;
    while(1)
    {
	time32_t nxt = days_per_month[leap_year(year)][month];
	if(cal < nxt)
	    break;
	cal -= nxt;
	month++;
    }
    tm->month_name = month_names[month];
    tm->month_abbrev = month_abbrevs[month];
    tm->month = ++month;
    tm->day = ++cal;

    /* Now calculate the day of the week, this algorithm is from Dr.Dobb's
       journal (April '95) by Kim S. Larsen. */
    if(month <= 2)
	month += 12, year--;
    tm->day_of_week = (cal + 2*month + 3*(month+1)/5 + year/4
		       - year/100 + year/400) % 7;
    tm->dow_name = dow_names[tm->day_of_week];
    tm->dow_abbrev = dow_abbrevs[tm->day_of_week];
}

#ifndef TEST
void
init_time(void)
{
    volatile u_long jop_ticks;
    if(!alloc_irq(0, timer_intr, "timer"))
    {
	kprintf("Can't get timer IRQ!\n");
        return;
    }
    if(!alloc_irq(8, cmos_timer, "cmos timer"))
    {
        kprintf("Can't get cmos timer IRQ!\n");
        return;
    }
    CMOS_WRITE(0x52, 0xb);
    kprintf("Calibrating delay loop: ");
    delay_loop_counter = 1;
    while(delay_loop_counter) {
        jop_ticks = timer_ticks;
        while(jop_ticks == timer_ticks);	/* sync em up */
        jop_ticks = timer_ticks;
        __delay(delay_loop_counter);
        jop_ticks = timer_ticks - jop_ticks;
        if(jop_ticks >= 32) {
            /* 1/32 of a second has elapsed */
            mums = delay_loop_counter << 5;
            /* mums = count for 1 second of delay */
            printk("%d.%0d made-up-mips...",
                mums / 1000000, (mums / 10000) % 100);
            return;
        }
        delay_loop_counter <<= 1;
    }
    printk("failed...");
}
#endif

#ifdef TEST_TIME
int
main(int argc, char **argv)
{
    time32_t date = current_time();
    struct time_bits tm;
    expand_time(date, &tm);
    printf("%d => %02d/%02d/%4d %02d:%02d:%02d\n",
	   (int)date, tm.day + 1, tm.month + 1, tm.year,
	   tm.hour, tm.minute, tm.second);
    return 0;
}
#endif

/*
 * Local Variables:
 * compile-command: "gcc -O2 -g -I.. -DTEST_TIME time.c -o time"
 * End:
 */
