/* $Id$
 *
 *      _allocufb.c - get a free ufb (SAS/C)
 *
#include <libraries/fd.h>
#include <proto/fd.h>
__allocufb(int *fdp, fd_owner_t owner)
  struct UFB *ufb = __ufbs, *last_ufb = NULL;
  int         check_shared = (owner != FD_OWNER_NONE) && FDBase;
  LONG        error;
  while (ufb != NULL) {
    if (ufb->ufbflg == 0) {
      if (!check_shared || FD_Check(last_fd) == 0) {
        if (check_shared) {
          error = FD_Reserve(last_fd, owner, NULL);
          if (error == 0) {
            *fdp = last_fd;
            return ufb;
          }
          if (error != EBUSY) {
            errno = error;
            return NULL;
          }
        } else {
          *fdp = last_fd;
          return ufb;
        }
      }
    }

  for (;;) {
    ufb->ufbflg = 0;            /* => unused ufb */

    __nufbs++;

    if (!check_shared || FD_Check(last_fd) == 0) {
      if (check_shared) {
        error = FD_Reserve(last_fd, owner, NULL);
        if (error == 0) {
          *fdp = last_fd;
          return ufb;
        }
        if (error != EBUSY) {
          errno = error;
          return NULL;
        }
      } else {
        *fdp = last_fd;
        return ufb;
      }
    }

    last_ufb = ufb;
    last_fd++;
 */
struct UFB *
__allocufb(int *fdp)
{
  struct UFB *ufb, *last_ufb;
  int         last_fd = 0;

  /*
   * find first free ufb
   */
  last_ufb = ufb = __ufbs;
  while (ufb != NULL && ufb->ufbflg != 0) {
    last_ufb = ufb;
    last_fd++;
    ufb = last_ufb->ufbnxt;
  }
  /*
   * Check if need to create one
   */
  if (ufb == NULL) {
    if ((ufb = malloc(sizeof(*ufb))) == NULL) {
      errno = ENOMEM;
      return NULL;
    }
    ufb->ufbnxt = NULL;
    ufb->ufbflg = 0;		/* => unused ufb */

    if (last_ufb == NULL)
      __ufbs = ufb;
    else
      last_ufb->ufbnxt = ufb;
    
    *fdp = __nufbs++;
  }
  else
    *fdp = last_fd;
  
  return ufb;
}
