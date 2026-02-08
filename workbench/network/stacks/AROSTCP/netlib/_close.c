/* $Id$
 *
 *      _close.c - close a file (SAS/C)
 *
 *      Copyright © 1994 AmiTCP/IP Group, 
 *                       Network Solutions Development Inc.
 *                       All rights reserved.
 */

#include <ios1.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <proto/dos.h>
#include <errno.h>
#include <bsdsocket.h>
#include <libraries/fd.h>
#include <proto/fd.h>

extern fd_type_t NetlibFDType;
extern struct Library *FDBase;

int 
__close(int fd)
{
  struct UFB *ufb;
  int is_socket;
  fd_type_t owner;

  if (FDBase && NetlibFDType != FD_TYPE_NONE) {
    owner = FD_GetOwner(fd);
    if (owner != FD_TYPE_NONE && owner != NetlibFDType) {
      LONG error = FD_Close(fd);
      if (error) {
        errno = error;
        return -1;
      }
      return 0;
    }
  }

  /*
   * Check for the break signals
   */
  __chkabort();

  /*
   * Find the ufb *
   */
  if ((ufb = __chkufb(fd)) == NULL) {
    /* __chkufb sets the errno to EBADF */
    return -1;
  }

  /*
   * Check if close is not needed
   */
  if ((ufb->ufbflg & (UFB_NC | UFB_CLO)) != UFB_NC) {

    /*
     * Empty flags mean empty ufb
     */
    if (ufb->ufbflg == 0) {
      errno = EBADF;
      return -1;
    }

    /*
     * Close the file
     */
    if (!(ufb->ufbflg & UFB_SOCK) && ufb->ufbfh != NULL) {
      Close(ufb->ufbfh);
      
      /*
       * Remove the file if it was temporary
       */
      if (ufb->ufbflg & UFB_TEMP && ufb->ufbfn != NULL) 
	remove(ufb->ufbfn);
    }

  }

  /*
   * Free the file name
   */
  if (ufb->ufbfn != NULL) {
    free(ufb->ufbfn);
    ufb->ufbfn = NULL;
  }

  is_socket = (ufb->ufbflg & UFB_SOCK);

  /*
   * Clear the flags to free this ufb
   */
  ufb->ufbflg = 0;
  ufb->ufbfh = NULL; /* just in case */

  if (!is_socket && FDBase && NetlibFDType != FD_TYPE_NONE) {
    FD_Free(fd, NetlibFDType);
  }

  /* 
   * closes the socket OR the file mark
   */
  CloseSocket(fd);
  
  return 0;
}
