/* $Id$
 *
 *      autoinit.c - SAS/C autoinitialization functions for bsdsocket.library
 *
 *      Copyright © 1994 AmiTCP/IP Group, 
 *                       Network Solutions Development Inc.
 *                       All rights reserved.
 *	Copyright © 2005 - 2011 Pavel Fedin
 */

#include <bsdsocket/socketbasetags.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <proto/socket.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/fd.h>
#include <libraries/fd.h>

#include <stdlib.h>
#include <errno.h>

#include "conf.h"

struct Library *SocketBase = NULL;
struct Library *FDBase = NULL;
fd_type_t NetlibFDType = FD_TYPE_NONE;

extern int __close(int fd);
extern int __read(int fd, void *buffer, unsigned int length);
extern int __write(int fd, const void *buffer, unsigned int length);
extern int ioctl(int fd, unsigned int request, char *argp);

static LONG __netlib_fd_close(LONG fd, APTR data)
{
  (void)data;
  if (__close(fd) < 0)
    return errno;
  return 0;
}

static LONG __netlib_fd_read(LONG fd, APTR data, void *buffer, ULONG length, ULONG *out_count)
{
  int result;

  (void)data;
  if (!out_count)
    return EINVAL;

  result = __read(fd, buffer, (unsigned int)length);
  if (result < 0)
    return errno;

  *out_count = (ULONG)result;
  return 0;
}

static LONG __netlib_fd_write(LONG fd, APTR data, const void *buffer, ULONG length, ULONG *out_count)
{
  int result;

  (void)data;
  if (!out_count)
    return EINVAL;

  result = __write(fd, buffer, (unsigned int)length);
  if (result < 0)
    return errno;

  *out_count = (ULONG)result;
  return 0;
}

static LONG __netlib_fd_ioctl(LONG fd, APTR data, ULONG request, APTR arg, LONG *out_result)
{
  int result;

  (void)data;
  if (!out_result)
    return EINVAL;

  result = ioctl(fd, request, (char *)arg);
  if (result != 0)
    return errno ? errno : result;

  *out_result = result;
  return 0;
}

static const char SOCKETNAME[] = "bsdsocket.library";

#define SOCKETVERSION 4		/* minimum bsdsocket version to use */

extern STRPTR _ProgramName;	/* startup module defines this :-) */

/****** net.lib/autoinit *********************************************

    NAME
        autoinit - Autoinitialization Functions

    SYNOPSIS
        LONG _STI_200_openSockets(void)

        void _STD_200_closeSockets(void)

    FUNCTION
        These functions open and close the bsdsocket.library at the startup
        and exit of the program, respectively.  For a program to use these
        functions, it must be linked with netlib:net.lib (or some variant).
        These functions are linked in only if the program references the
        global symbol "SocketBase".

        If the library can be opened, the _STI_200_openSockets() calls
        bsdsocket.library function SocketBaseTags() to tell the library the
        address and the size of the errno variable of the calling program,
        the program name (to be used in syslog() messages) and the address
        of the h_error variable (in which the name resolver errors are
        returned).

    NOTES
        _STI_200_openSockets() also checks that the system version is at
        least 37. It also puts up a requester if the bsdsocket.library is
        not found or is of wrong version.

        The autoinitialization and autotermination functions are features
        specific to the SAS C6.  However, these functions can be used with
        other (ANSI) C compilers, too. Example follows:

        \* at start of main() *\

        atexit(_STD_200_closeSockets);
        if (_STI_200_openSockets() != 0)
	   exit(20);

    BUGS
        The same autoinitialization won't work for both SAS C 6.3 and SAS C
        6.50 or latter.  Only way to terminate an initialization function is
        by exit() call with SAS C 6.3 binary.  If an autoinitialization
        function is terminated by exit() call with SAS C 6.50 binary, the
        autotermination functions won't be called.  Due this braindamage
        these compilers require separate net.lib libraries.

    SEE ALSO
        bsdsocket.library/SocketBaseTags(),
        SAS/C 6 User's Guide p. 145 for details of autoinitialization and
        autotermination functions.

*****************************************************************************
*/

/* SAS C 6.50 kludge */
#ifdef SASC
#if __VERSION__ > 6 || __REVISION__ >= 50
#define exit(x) return x
#endif
#endif

/* AROS kludge, see below */
#ifdef __AROS__

#include <aros/symbolsets.h>

#undef CONSTRUCTOR
#define CONSTRUCTOR
#define exit(x) return x
#endif

/*
 * Using __stdargs prevents creation of register arguments entry point.
 * If both stack args and reg. args entry points are created, this
 * function is called _twice_, which is not wanted.
 *
 * The number 200 in the function names is the priority assigned to
 * shared library autoinitialization functions by SAS/C 6.50.
 */
LONG STDARGS CONSTRUCTOR _STI_200_openSockets(void)
{
  struct Library *IntuitionBase;

  /*
   * Open bsdsocket.library
   */
  if ((SocketBase = OpenLibrary((STRPTR)SOCKETNAME, SOCKETVERSION)) != NULL) {
    FDBase = OpenLibrary("fd.library", 0);
    if (FDBase) {
      struct fd_hooks hooks = {
        .fd_close = __netlib_fd_close,
        .fd_read = __netlib_fd_read,
        .fd_write = __netlib_fd_write,
        .fd_ioctl = __netlib_fd_ioctl,
      };

      if (FD_RegisterType(&hooks, &NetlibFDType) != 0)
        NetlibFDType = FD_TYPE_NONE;
    }
    /*
     * Succesfull. Now tell bsdsocket.library:
     * - the address of our errno
     * - our program name
     * h_errno is not processed here any more, now it's thread-safe
     */
    if (SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), &errno,
		       SBTM_SETVAL(SBTC_LOGTAGPTR), _ProgramName,
		       TAG_END))
      exit(30);
    return 0;
  }

  IntuitionBase = OpenLibrary("intuition.library", 36);

  if (IntuitionBase != NULL) {
    struct EasyStruct libraryES;

    libraryES.es_StructSize = sizeof(libraryES);
    libraryES.es_Flags = 0;
    libraryES.es_Title = _ProgramName;
    libraryES.es_TextFormat = "Unable to open bsdsocket.library version 4 or later";
    libraryES.es_GadgetFormat = "Exit %s";

    EasyRequestArgs(NULL, &libraryES, NULL, (APTR)&_ProgramName);

    CloseLibrary(IntuitionBase);
  }
  exit(20);
}

void STDARGS DESTRUCTOR _STD_200_closeSockets(void)
{
  if (SocketBase) {
    CloseLibrary(SocketBase);
    SocketBase = NULL;
  }
  if (FDBase) {
    CloseLibrary(FDBase);
    FDBase = NULL;
  }
  NetlibFDType = FD_TYPE_NONE;
}

#ifdef __AROS__

/*
 * Unfortunately AROS startup code does not support early exit().
 * This can be considered a serious C++ compatibility problem since
 * constructors of static objects may throw exceptions,
 * involving exit() or abort() calls.
 */

static ULONG AROS_openSockets(void)
{
    return _STI_200_openSockets() ? FALSE : TRUE;
}

ADD2INIT(AROS_openSockets, 10);
ADD2EXIT(_STD_200_closeSockets, 10);

#endif
