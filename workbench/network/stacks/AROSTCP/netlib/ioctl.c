/* $Id$
 *
 *      ioctl.c - set file control information
 *
 *      Copyright © 1994 AmiTCP/IP Group, 
 *                       Network Solutions Development Inc.
 *                       All rights reserved.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <libraries/fd.h>
#include <proto/fd.h>
#include <errno.h>

extern struct Library *FDBase;
extern fd_type_t NetlibFDType;

int ioctl(int fd, unsigned int request, char *argp)
{
  int success;
  fd_type_t owner;

  /*
   * IoctlSocket will return EBADF if the d is not socket
   */
  if (FDBase && NetlibFDType != FD_TYPE_NONE) {
    owner = FD_GetOwner(fd);
    if (owner != FD_TYPE_NONE && owner != NetlibFDType) {
      LONG result = 0;
      LONG error = FD_Ioctl(fd, request, argp, &result);
      if (error) {
        errno = error;
        return error;
      }
      return (int)result;
    }
  }

  success = IoctlSocket(fd, request, argp);

  /*
   * Maybe the EBADF should be converted to EINVAL if the fd is an usual file?
   */

  return success;
}
