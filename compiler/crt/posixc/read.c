/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.

    POSIX.1-2008 function read().
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

        ssize_t read (

/*  SYNOPSIS */
        int    fd,
        void * buf,
        size_t count)

/*  FUNCTION
        Read an amount of bytes from a file descriptor.

    INPUTS
        fd - The file descriptor to read from
        buf - The buffer to read the bytes into
        count - Read this many bytes.

    RESULT
        The number of characters read (may range from 0 when the file
        descriptor contains no more characters to count) or -1 on error.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        open(), read(), fread()

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
        error = FD_Read(fd, buf, count, &out_count);
        if (error) {
            errno = error;
            return -1;
        }
        return (ssize_t)out_count;
    }

    error = __posixc_read(fd, buf, count, &out_count);
    if (error) {
        errno = error;
        return -1;
    }

    return (ssize_t)out_count;
} /* read */

LONG __posixc_read(int fd, void *buf, size_t count, ULONG *out_count)
{
    fdesc *fdesc = __getfdesc(fd);
    ssize_t cnt;

    if (!out_count)
        return EINVAL;

    if (!fdesc)
        return EBADF;

    if (fdesc->fcb->privflags & _FCB_ISDIR)
        return EISDIR;

    cnt = Read(fdesc->fcb->handle, buf, count);
    if (cnt == -1)
        return __stdc_ioerr2errno(IoErr());

    *out_count = (ULONG)cnt;
    return 0;
}
