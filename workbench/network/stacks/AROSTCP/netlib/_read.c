/* $Id$
 *
 *      _read.c - read() for both files and sockets (SAS/C)
 *
 *      Copyright © 1994 AmiTCP/IP Group, 
 *                       Network Solutions Development Inc.
 *                       All rights reserved.
 */

#include <ios1.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <errno.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include <bsdsocket.h>
#include <libraries/fd.h>
#include <proto/fd.h>

extern int __io2errno(long);
extern struct Library *FDBase;
extern fd_type_t NetlibFDType;

int
__read(int fd, void *buffer, unsigned int length)
{
  struct UFB *ufb;
  int         count;
  char        ch, *ptr, *nextptr, *endptr;
  fd_type_t   owner;

  /*
   * Check for the break signals
   */
  __chkabort();
  if (FDBase && NetlibFDType != FD_TYPE_NONE) {
    owner = FD_GetOwner(fd);
    if (owner != FD_TYPE_NONE && owner != NetlibFDType) {
      ULONG out_count = 0;
      LONG error = FD_Read(fd, buffer, length, &out_count);
      if (error) {
        errno = error;
        return -1;
      }
      return (int)out_count;
    }
  }

  /*
   * find the ufb *
   */
  if ((ufb = __chkufb(fd)) == NULL) {
    errno = EINVAL;
    return -1;
  }
  /*
   * Check if read is allowed
   */
  if (!(ufb->ufbflg & UFB_RA)) {
    _OSERR = ERROR_READ_PROTECTED;
    errno = EIO;
    return -1;
  }

  /*
   * Do the Actual read
   */
  _OSERR = 0;
  if (ufb->ufbflg & UFB_SOCK) {
    if ((count = recv(fd, (UBYTE *)buffer, length, 0)) < 0) {
      return -1;
    }
  }
  else {
    if ((count = Read(ufb->ufbfh, buffer, length)) == -1) {
      errno = __io2errno(_OSERR = IoErr());
      return -1;
    }
  }
  /*
   * Check if translation is not needed
   */
  if (count == 0 || !(ufb->ufbflg & UFB_XLAT))
    return count;
  
  endptr = (char *)buffer + count;    /* first point NOT in buffer */
  
  if (endptr[-1] == 0x0D)	/* checks last char */
#if 0
    Read(ufb->ufbfh, buffer, 1); /* overwrites first read byte ! */
  /*            BUG  ^^^^^^ */
#else
    count--, endptr--;
#endif

  /*
   * Remove 0x0D's (CR) (This doesn't remove a CR at the end of the buffer).
   */
  if ((ptr = memchr(buffer, 0x0D, count)) != NULL) {
    nextptr = ptr + 1;
    while (nextptr < endptr) {
      if ((ch = *nextptr) != 0x0D)
	*ptr++ = ch;
      nextptr++;
    }
    count = ptr - (char *)buffer;
  }

  /*
   * Test for CTRL-Z (end of file)
   */
  if ((ptr = memchr(buffer, 0x1A, count)) == NULL)
    return count;

  return ptr - (char *)buffer;
}
