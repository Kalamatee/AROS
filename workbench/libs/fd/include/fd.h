#ifndef FD_LIBRARY_H
#define FD_LIBRARY_H

/*
    Copyright (C) 2025-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UWORD fd_type_t;
typedef fd_type_t fd_owner_t;

#define FD_TYPE_NONE ((fd_type_t)0)

struct fd_hooks {
    LONG (*fd_close)(LONG fd, APTR data);
    LONG (*fd_read)(LONG fd, APTR data, void *buffer, ULONG length, ULONG *out_count);
    LONG (*fd_write)(LONG fd, APTR data, const void *buffer, ULONG length, ULONG *out_count);
    LONG (*fd_ioctl)(LONG fd, APTR data, ULONG request, APTR arg, LONG *out_result);
};

LONG FD_RegisterType(const struct fd_hooks *hooks, fd_type_t *out_type);
LONG FD_Alloc(LONG startfd, fd_type_t type, APTR data, LONG *outfd);
LONG FD_Reserve(LONG fd, fd_type_t type, APTR data);
LONG FD_Free(LONG fd, fd_type_t type);
LONG FD_Check(LONG fd);
fd_type_t FD_GetOwner(LONG fd);
#define FD_GetType(fd) FD_GetOwner(fd)
APTR FD_GetData(LONG fd);
LONG FD_SetData(LONG fd, fd_type_t type, APTR data);
LONG FD_Close(LONG fd);
LONG FD_Read(LONG fd, void *buffer, ULONG length, ULONG *out_count);
LONG FD_Write(LONG fd, const void *buffer, ULONG length, ULONG *out_count);
LONG FD_Ioctl(LONG fd, ULONG request, APTR arg, LONG *out_result);

#ifdef __cplusplus
}
#endif

#endif /* FD_LIBRARY_H */
