/* errno.c -- Errno handling.
   John Harper. */

#include <vmm/types.h>
#include <vmm/errno.h>

#ifdef TEST
int vmm_errno;
#endif

const char *error_table[] = {
    "No error.",
    "File or directory doesn't exist.",
    "Device doesn't exist.",
    "Name is not unique.",
    "Access denied.",
    "Object is not a directory.",
    "Object is a directory.",
    "No space on the device.",
    "Directory is not empty.",
    "Device is read-only.",
    "Bad magic number on device.",
    "End of file.",
    "Disk has been changed.",
    "Data error.",
    "No disk in device.",
    "Invalid parameter.",
    "Can't link across devices.",
    "No memory available.",
    "Invalid inode/device.",
    "Object in use.",
    "Not a symbolic link.",
    "Too many symbolic links encountered.",
};

int max_err = E_MAXLINKS;

/* Return a string describing the error ERRNO. */
const char *
error_string(int errno)
{
    if(errno < 0)
	errno = -errno;
    if(errno > max_err)
	return "Unknown error.";
    return error_table[errno];
}
