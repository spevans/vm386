/* errno.h -- System-wide error values.
   John Harper. */

#ifndef __VMM_ERRNO_H
#define __VMM_ERRNO_H

#ifndef TEST
# ifdef KERNEL
#  define ERRNO (current_task->errno)
# else
#  define ERRNO (kernel->current_task->errno)
# endif
#else
# define ERRNO vmm_errno
extern int vmm_errno;
#endif

#define E_OK		0	/* No error. */
#define E_NOEXIST	1	/* Name doesn't exist. */
#define E_NODEV		2	/* Device doesn't exist. */
#define E_EXISTS	3	/* Name non-unique. */
#define E_PERM		4	/* No permission. */
#define E_NOTDIR	5	/* Not a directory. */
#define E_ISDIR		6	/* Is a directory. */
#define E_NOSPC		7	/* No space on device. */
#define E_NOTEMPTY	8	/* Directory not empty. */
#define E_RO		9	/* Read-only device. */
#define E_BADMAGIC	10	/* Magic number error. */
#define E_EOF		11	/* End of file. */
#define E_DISKCHANGE	12	/* The disk was changed. */
#define E_IO		13	/* Some kind of data error. */
#define E_NODISK	14	/* Drive is empty. */
#define E_BADARG	15	/* Invalid argument to a function. */
#define E_XDEV		16	/* Can't have links across devices. */
#define E_NOMEM		17	/* Out of memory. */
#define E_INVALID	18	/* Inode/device has been invalidated. */
#define E_INUSE		19	/* Object is being referenced. */
#define E_NOTLINK	20	/* Object isn't a symbolic link. */
#define E_MAXLINKS	21	/* Too many symbolic links. */

#ifdef KERNEL
extern const char *error_string(int errno);
extern const char *error_table[];
extern int max_err;
#endif

#endif /* __VMM_ERRNO_H */
