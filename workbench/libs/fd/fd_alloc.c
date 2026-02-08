/*
    Copyright (C) 2025-2026, The AROS Development Team. All rights reserved.
*/

#include <aros/libcall.h>
#include <proto/exec.h>
#include <errno.h>

#include "fd_private.h"
#include LC_LIBDEFS_FILE

static LONG fd_ensure_capacity(struct fd_base *base, ULONG min_slots)
{
    if (min_slots <= base->fd_Slots)
        return 0;

    if (min_slots == 0)
        return EINVAL;

    fd_entry *new_table = AllocVec(min_slots * sizeof(*new_table), MEMF_ANY | MEMF_CLEAR);
    if (!new_table)
        return ENOMEM;

    if (base->fd_Table) {
        CopyMem(base->fd_Table, new_table, base->fd_Slots * sizeof(*new_table));
        FreeVec(base->fd_Table);
    }

    base->fd_Table = new_table;
    base->fd_Slots = min_slots;
    return 0;
}

static fd_entry *fd_get_entry(struct fd_base *base, LONG fd)
{
    if (fd < 0 || (ULONG)fd >= base->fd_Slots)
        return NULL;
    return &base->fd_Table[fd];
}

static const struct fd_hooks *fd_get_hooks(struct fd_base *base, fd_type_t type)
{
    if (type == FD_TYPE_NONE)
        return NULL;
    if (type > base->fd_Types)
        return NULL;
    return &base->fd_Hooks[type - 1];
}

/*****************************************************************************

    NAME */
        AROS_LH2(LONG, FD_RegisterType,

/*  SYNOPSIS */
        AROS_LHA(const struct fd_hooks *, hooks, A0),
        AROS_LHA(fd_type_t *, out_type, A1),

/*  LOCATION */
        struct fd_base *, FDBase, 4, FD)

/*  FUNCTION
        Register a new file descriptor handler type and return its type id.

    INPUTS
        hooks    - Hook table for descriptor operations.
        out_type - Pointer that receives the registered type id.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Alloc()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    struct fd_hooks *new_hooks = NULL;
    fd_type_t new_type = 0;

    if (!hooks || !out_type)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);

    if (FDBase->fd_Types == (fd_type_t)~0) {
        error = ENOMEM;
        goto done;
    }

    new_type = FDBase->fd_Types + 1;
    new_hooks = AllocVec(new_type * sizeof(*new_hooks), MEMF_ANY | MEMF_CLEAR);
    if (!new_hooks) {
        error = ENOMEM;
        goto done;
    }

    if (FDBase->fd_Hooks) {
        CopyMem(FDBase->fd_Hooks, new_hooks, FDBase->fd_Types * sizeof(*new_hooks));
        FreeVec(FDBase->fd_Hooks);
    }

    new_hooks[new_type - 1] = *hooks;
    FDBase->fd_Hooks = new_hooks;
    FDBase->fd_Types = new_type;
    *out_type = new_type;

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH1(LONG, FD_Close,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),

/*  LOCATION */
        struct fd_base *, FDBase, 12, FD)

/*  FUNCTION
        Invoke the close hook for the descriptor.

    INPUTS
        fd - Descriptor to close.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Read(), FD_Write(), FD_Ioctl()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    fd_entry *entry = NULL;
    fd_type_t type = FD_TYPE_NONE;
    APTR data = NULL;
    const struct fd_hooks *hooks;

    ObtainSemaphore(&FDBase->fd_Lock);
    entry = fd_get_entry(FDBase, fd);
    if (entry) {
        type = entry->owner;
        data = entry->data;
    }
    ReleaseSemaphore(&FDBase->fd_Lock);

    if (type == FD_TYPE_NONE)
        return EBADF;

    hooks = fd_get_hooks(FDBase, type);
    if (!hooks || !hooks->fd_close)
        return ENOSYS;

    return hooks->fd_close(fd, data);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH4(LONG, FD_Read,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(void *, buffer, A0),
        AROS_LHA(ULONG, length, D1),
        AROS_LHA(ULONG *, out_count, A1),

/*  LOCATION */
        struct fd_base *, FDBase, 13, FD)

/*  FUNCTION
        Invoke the read hook for the descriptor.

    INPUTS
        fd        - Descriptor to read from.
        buffer    - Output buffer.
        length    - Amount of bytes to read.
        out_count - Pointer receiving the number of bytes read.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Write(), FD_Ioctl(), FD_Close()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    fd_entry *entry = NULL;
    fd_type_t type = FD_TYPE_NONE;
    APTR data = NULL;
    const struct fd_hooks *hooks;

    if (!out_count)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);
    entry = fd_get_entry(FDBase, fd);
    if (entry) {
        type = entry->owner;
        data = entry->data;
    }
    ReleaseSemaphore(&FDBase->fd_Lock);

    if (type == FD_TYPE_NONE)
        return EBADF;

    hooks = fd_get_hooks(FDBase, type);
    if (!hooks || !hooks->fd_read)
        return ENOSYS;

    return hooks->fd_read(fd, data, buffer, length, out_count);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH4(LONG, FD_Write,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(const void *, buffer, A0),
        AROS_LHA(ULONG, length, D1),
        AROS_LHA(ULONG *, out_count, A1),

/*  LOCATION */
        struct fd_base *, FDBase, 14, FD)

/*  FUNCTION
        Invoke the write hook for the descriptor.

    INPUTS
        fd        - Descriptor to write to.
        buffer    - Input buffer.
        length    - Amount of bytes to write.
        out_count - Pointer receiving the number of bytes written.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Read(), FD_Ioctl(), FD_Close()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    fd_entry *entry = NULL;
    fd_type_t type = FD_TYPE_NONE;
    APTR data = NULL;
    const struct fd_hooks *hooks;

    if (!out_count)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);
    entry = fd_get_entry(FDBase, fd);
    if (entry) {
        type = entry->owner;
        data = entry->data;
    }
    ReleaseSemaphore(&FDBase->fd_Lock);

    if (type == FD_TYPE_NONE)
        return EBADF;

    hooks = fd_get_hooks(FDBase, type);
    if (!hooks || !hooks->fd_write)
        return ENOSYS;

    return hooks->fd_write(fd, data, buffer, length, out_count);

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH4(LONG, FD_Ioctl,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(ULONG, request, D1),
        AROS_LHA(APTR, arg, A0),
        AROS_LHA(LONG *, out_result, A1),

/*  LOCATION */
        struct fd_base *, FDBase, 15, FD)

/*  FUNCTION
        Invoke the ioctl hook for the descriptor.

    INPUTS
        fd         - Descriptor to control.
        request    - Ioctl request.
        arg        - Request specific argument.
        out_result - Pointer receiving ioctl result.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Read(), FD_Write(), FD_Close()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    fd_entry *entry = NULL;
    fd_type_t type = FD_TYPE_NONE;
    APTR data = NULL;
    const struct fd_hooks *hooks;

    if (!out_result)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);
    entry = fd_get_entry(FDBase, fd);
    if (entry) {
        type = entry->owner;
        data = entry->data;
    }
    ReleaseSemaphore(&FDBase->fd_Lock);

    if (type == FD_TYPE_NONE)
        return EBADF;

    hooks = fd_get_hooks(FDBase, type);
    if (!hooks || !hooks->fd_ioctl)
        return ENOSYS;

    return hooks->fd_ioctl(fd, data, request, arg, out_result);

    AROS_LIBFUNC_EXIT
}
/*****************************************************************************

    NAME */
        AROS_LH4(LONG, FD_Alloc,

/*  SYNOPSIS */
        AROS_LHA(LONG, startfd, D0),
        AROS_LHA(fd_type_t, type, D1),
        AROS_LHA(APTR, data, A0),
        AROS_LHA(LONG *, outfd, A1),

/*  LOCATION */
        struct fd_base *, FDBase, 5, FD)

/*  FUNCTION
        Allocate a file descriptor slot for the specified handler type.

    INPUTS
        startfd - Starting descriptor number to search from.
        type    - Descriptor handler type identifier.
        data    - Optional owner data.
        outfd   - Pointer that receives the allocated descriptor.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Reserve()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    LONG fd = startfd;
    fd_entry *entry = NULL;

    if (!outfd)
        return EINVAL;

    if (type == FD_TYPE_NONE)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);

    if (!fd_get_hooks(FDBase, type)) {
        error = EINVAL;
        goto done;
    }

    if (fd < 0) {
        error = EBADF;
        goto done;
    }

    for (;; fd++) {
        if ((ULONG)fd >= FDBase->fd_Slots) {
            error = fd_ensure_capacity(FDBase, fd + 1);
            if (error)
                goto done;
        }

        entry = &FDBase->fd_Table[fd];
        if (entry->owner == FD_TYPE_NONE) {
            entry->owner = type;
            entry->data = data;
            *outfd = fd;
            goto done;
        }
    }

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH3(LONG, FD_Reserve,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(fd_type_t, type, D1),
        AROS_LHA(APTR, data, A0),

/*  LOCATION */
        struct fd_base *, FDBase, 6, FD)

/*  FUNCTION
        Reserve a specific file descriptor slot for the specified handler type.

    INPUTS
        fd     - Descriptor number to reserve.
        type   - Descriptor handler type identifier.
        data   - Optional owner data.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Alloc()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    fd_entry *entry = NULL;

    if (type == FD_TYPE_NONE)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);

    if (!fd_get_hooks(FDBase, type)) {
        error = EINVAL;
        goto done;
    }

    if (fd < 0) {
        error = EBADF;
        goto done;
    }

    error = fd_ensure_capacity(FDBase, fd + 1);
    if (error)
        goto done;

    entry = &FDBase->fd_Table[fd];
    if (entry->owner != FD_TYPE_NONE) {
        error = EBUSY;
        goto done;
    }

    entry->owner = type;
    entry->data = data;

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH2(LONG, FD_Free,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(fd_type_t, type, D1),

/*  LOCATION */
        struct fd_base *, FDBase, 7, FD)

/*  FUNCTION
        Release a descriptor slot owned by a specific handler type.

    INPUTS
        fd     - Descriptor number to release.
        type   - Descriptor handler type identifier.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Reserve()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    fd_entry *entry = NULL;

    if (type == FD_TYPE_NONE)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);

    entry = fd_get_entry(FDBase, fd);
    if (!entry || entry->owner == FD_TYPE_NONE || entry->owner != type) {
        error = EBADF;
        goto done;
    }

    entry->owner = FD_TYPE_NONE;
    entry->data = NULL;

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH1(LONG, FD_Check,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),

/*  LOCATION */
        struct fd_base *, FDBase, 8, FD)

/*  FUNCTION
        Check whether a descriptor slot is available.

    INPUTS
        fd - Descriptor number to check.

    RESULT
        0 if the slot is free, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_GetOwner()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    fd_entry *entry = NULL;

    ObtainSemaphore(&FDBase->fd_Lock);

    if (fd < 0) {
        error = EBADF;
        goto done;
    }

    entry = fd_get_entry(FDBase, fd);
    if (entry && entry->owner != FD_TYPE_NONE)
        error = EBUSY;

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH1(fd_owner_t, FD_GetOwner,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),

/*  LOCATION */
        struct fd_base *, FDBase, 9, FD)

/*  FUNCTION
        Query the owner of a descriptor slot.

    INPUTS
        fd - Descriptor number to query.

    RESULT
        Owner identifier or FD_TYPE_NONE.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_Check()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    fd_owner_t owner = FD_TYPE_NONE;
    fd_entry *entry = NULL;

    ObtainSemaphore(&FDBase->fd_Lock);

    entry = fd_get_entry(FDBase, fd);
    if (entry)
        owner = entry->owner;

    ReleaseSemaphore(&FDBase->fd_Lock);
    return owner;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH1(APTR, FD_GetData,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),

/*  LOCATION */
        struct fd_base *, FDBase, 10, FD)

/*  FUNCTION
        Query the owner data for a descriptor slot.

    INPUTS
        fd - Descriptor number to query.

    RESULT
        Owner data pointer or NULL.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_SetData()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    APTR data = NULL;
    fd_entry *entry = NULL;

    ObtainSemaphore(&FDBase->fd_Lock);

    entry = fd_get_entry(FDBase, fd);
    if (entry)
        data = entry->data;

    ReleaseSemaphore(&FDBase->fd_Lock);
    return data;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
        AROS_LH3(LONG, FD_SetData,

/*  SYNOPSIS */
        AROS_LHA(LONG, fd, D0),
        AROS_LHA(fd_type_t, type, D1),
        AROS_LHA(APTR, data, A0),

/*  LOCATION */
        struct fd_base *, FDBase, 11, FD)

/*  FUNCTION
        Update the owner data for a descriptor slot.

    INPUTS
        fd    - Descriptor number to update.
        type  - Descriptor handler type identifier.
        data  - Owner data pointer.

    RESULT
        0 on success, or an errno-style error code.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FD_GetData()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    fd_entry *entry = NULL;

    if (type == FD_TYPE_NONE)
        return EINVAL;

    ObtainSemaphore(&FDBase->fd_Lock);

    entry = fd_get_entry(FDBase, fd);
    if (!entry || entry->owner != type) {
        error = EBADF;
        goto done;
    }

    entry->data = data;

 done:
    ReleaseSemaphore(&FDBase->fd_Lock);
    return error;

    AROS_LIBFUNC_EXIT
}
