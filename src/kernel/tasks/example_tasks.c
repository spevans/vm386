/* example_tasks.c - two example tasks to test out how a 386 does multitasking
   by Si
*/

#include <vmm/page.h>
#include <vmm/string.h>
#include <vmm/kernel.h>

extern volatile u_long timer_ticks;
static int task_index;

#define TASKX_MSG	"\
.................................This is task X.................................\
"

void example_task(void)
{
   u_long ticks;
   int offset = 0;
   int index;
   char msg[80];
   char *Scr = TO_LOGICAL(0xb8000, char *);
   int i;

   index = (task_index++) % 25;

   Scr += 160*index;
   strcpy(msg, TASKX_MSG); 
   msg[46] = 'A' + index;

   kprintf("example task running\n");
   while(1) { 
     for(i=0;i<80;i++) {
       *(Scr+(i<<1)) = *(msg + (i + offset) % 80);
	*(Scr+(i<<1)+1) = 7;
     }
     offset++;
     ticks = timer_ticks;
     while(ticks+2 >timer_ticks);
   }
}
