/*
    Copyright (C) 1995-2012, The AROS Development Team. All rights reserved.

    POSIX.1-2008 function write().
*/

#include <errno.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "__fdesc.h"
#include "__posixc_intbase.h"
#include <libraries/fd.h>
#include <proto/fd.h>

/*****************************************************************************

    NAME */
#include <unistd.h>

        ssize_t write (

/*  SYNOPSIS */
        int          fd,
        const void * buf,
        size_t       count)

/*  FUNCTION
        Write an amount of characters to the specified file descriptor.

    INPUTS
        fd - The file descriptor to write to
        buf - Write these bytes into the file descriptor
        count - Write that many bytes

    RESULT
        The number of characters written or -1 on error.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    ULONG out_count = 0;
    fd_type_t owner = __posixc_get_owner(fd);
    fd_type_t posixc_type = __posixc_get_fd_type();
    LONG error;

    if (posixc_type != FD_TYPE_NONE && owner != FD_TYPE_NONE && owner != posixc_type) {
        struct Library *FDBase = NULL;
        struct PosixCIntBase *PosixCBase =
            (struct PosixCIntBase *)__aros_getbase_PosixCBase();

        FDBase = PosixCBase->PosixCFDBase;
        error = FD_Write(fd, buf, count, &out_count);
        if (error) {
            errno = error;
            return -1;
        }
        return (ssize_t)out_count;
    }

    error = __posixc_write(fd, buf, count, &out_count);
    if (error) {
        errno = error;
        return -1;
    }

    return (ssize_t)out_count;
} /* write */

LONG __posixc_write(int fd, const void *buf, size_t count, ULONG *out_count)
{
    fdesc *fdesc = __getfdesc(fd);
    ssize_t cnt;

    if (!out_count)
        return EINVAL;

    if (!fdesc)
        return EBADF;

    cnt = Write(fdesc->fcb->handle, (void *)buf, count);
    if (cnt == -1)
        return __stdc_ioerr2errno(IoErr());

    *out_count = (ULONG)cnt;
    return 0;
}
