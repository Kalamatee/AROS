/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.

    POSIX.1-2008 function close().
*/

#include <unistd.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <errno.h>
#include "__fdesc.h"
#include "__posixc_intbase.h"
#include <libraries/fd.h>
#include <proto/fd.h>

/*****************************************************************************

    NAME */
#include <unistd.h>

        int close (

/*  SYNOPSIS */
        int fd)

/*  FUNCTION
        Closes an open file. If this is the last file descriptor
        associated with this file, then all allocated resources
        are freed, too.

    INPUTS
        fd - The result of a successful open()

    RESULT
        -1 for error or zero on success.

    NOTES
        This function must not be used in a shared library or
        in a threaded application.

    EXAMPLE

    BUGS

    SEE ALSO
        open(), read(), write(), fopen()

    INTERNALS

******************************************************************************/
{
    fd_type_t owner = __posixc_get_owner(fd);
    fd_type_t posixc_type = __posixc_get_fd_type();
    LONG error;

    if (posixc_type != FD_TYPE_NONE && owner != FD_TYPE_NONE && owner != posixc_type) {
        struct Library *FDBase = NULL;
        struct PosixCIntBase *PosixCBase =
            (struct PosixCIntBase *)__aros_getbase_PosixCBase();

        FDBase = PosixCBase->PosixCFDBase;
        error = FD_Close(fd);
        if (error) {
            errno = error;
            return -1;
        }
        return 0;
    }

    error = __posixc_close(fd);
    if (error) {
        errno = error;
        return -1;
    }

    return 0;
} /* close */

LONG __posixc_close(int fd)
{
    fdesc *fdesc;

    if (!(fdesc = __getfdesc(fd)))
        return EBADF;

    if (--fdesc->fcb->opencount == 0)
    {
        /* Due to a *stupid* behaviour of the dos.library we cannot handle closing failures cleanly :-(
        if (
            !(fdesc->fcb->privflags & _FCB_DONTCLOSE_FH) &&
            !Close(fdesc->fh)
        )
        {
            fdesc->opencount++;
            return __stdc_ioerr2errno(IoErr());
        }
        */
        /* FIXME: Damn dos.library! We cannot report the error code correctly! This oughta change someday... */
        /* Since the dos.library destroys the file handle anyway, even if the closing fails, we cannot
           report the error code correctly, so just close the file and get out of here */

        if (!(fdesc->fcb->privflags & _FCB_DONTCLOSE_FH))
        {
            // don't close directories because we don't Open() them.
            if (fdesc->fcb->privflags & _FCB_ISDIR)
            {
                UnLock(fdesc->fcb->handle);
            }
            else
            {
                Close(fdesc->fcb->handle);
            }
        }

        FreeVec(fdesc->fcb);
    }

    __free_fdesc(fdesc);
    __setfdesc(fd, NULL);

    return 0;
}
