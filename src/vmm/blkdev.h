/* blkdev.h -- Generic block device handling.

   Before including this file #define the two symbols BLKDEV_TYPE to the
   type of the device struct and BLKDEV_NAME to a string naming the
   device.

   John Harper. */

#ifndef __VMM_BLKDEV_H
#define __VMM_BLKDEV_H

#include <vmm/lists.h>
#include <vmm/tasks.h>

typedef struct {
    list_node_t node;
    BLKDEV_TYPE *dev;			/* Device to access */
    char *buf;				/* Block being read/written */
    u_long block;			/* Index of block being read/written */
    int nblocks;			/* Number of blocks to read */
    int command;			/* Device-specific command. */
    int result;				/* := result of request */
    char retries;			/* Times we tried to do this command */
    bool completed;			/* TRUE when request has finished */
    struct semaphore sem;		/* Task locked on this request. */
} blkreq_t;

/* Called when the current request is completed. RESULT is the value to
   stash in the request. This can be called from an interrupt or the
   normal kernel context.

   The macro argument CUR-REQ-VAR should be the name of the variable used
   by the driver to store its current request. */

#define END_REQUEST_FUN(cur_req_var)				\
static void							\
end_request(int result)						\
{								\
    u_long flags;						\
    DB((BLKDEV_NAME ":end_request: result=%d\n", result));	\
    save_flags(flags);						\
    cli();							\
    if(cur_req_var != NULL)					\
    {								\
	cur_req_var->completed = TRUE;				\
	cur_req_var->result = result;				\
	signal(&cur_req_var->sem);				\
	cur_req_var = NULL;					\
    }								\
    load_flags(flags);						\
}

/* Invoke REQ then wait for it to complete.

   The macro argument DO-REQ-FUN names the function used to invoke the
   next request. */

#define SYNC_REQUEST_FUN(do_req_fun)				\
static inline bool						\
sync_request(blkreq_t *req)					\
{								\
    DB((BLKDEV_NAME ":sync_request: req=%p\n", req));		\
    set_sem_blocked(&req->sem);					\
    req->completed = FALSE;					\
    req->retries = 0;						\
    do_req_fun(req);						\
    wait(&req->sem);						\
    return req->result == 0;					\
}

/* Invoke REQ.

   The macro argument DO-REQ-FUN names the function used to invoke the
   next request. */

#define ASYNC_REQUEST_FUN(do_req_fun)				\
static inline void						\
async_request(blkreq_t *req)					\
{								\
    DB((BLKDEV_NAME ":async_request: req=%p\n", req));		\
    set_sem_blocked(&req->sem);					\
    req->completed = FALSE;					\
    req->retries = 0;						\
    do_req_fun(req);						\
}

#endif /* __VMM_BLKDEV_H */
