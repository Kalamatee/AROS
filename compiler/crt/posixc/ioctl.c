/*
    Copyright (C) 2004-2016, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <unistd.h>

#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <sys/ioctl.h>
#include <proto/dos.h>

#include <errno.h>

#include "__fdesc.h"
#include "__posixc_intbase.h"
#include <libraries/fd.h>
#include <proto/fd.h>

static int send_action_packet(struct MsgPort *port, BPTR arg)
{
    struct MsgPort *replyport;
    struct StandardPacket *packet;
    SIPTR ret;

    replyport = CreatePort(NULL,0L);
    if(!replyport)
    {
        return 0;
    }
    packet = (struct StandardPacket *) AllocMem(sizeof(struct StandardPacket),
                                                MEMF_PUBLIC|MEMF_CLEAR);
    if(!packet)
    {
        DeletePort(replyport);
        return 0;
    }

    packet->sp_Msg.mn_Node.ln_Name = (char *)&(packet->sp_Pkt);
    packet->sp_Pkt.dp_Link         = &(packet->sp_Msg);
    packet->sp_Pkt.dp_Port         = replyport;
    packet->sp_Pkt.dp_Type         = ACTION_DISK_INFO;
    packet->sp_Pkt.dp_Arg1         = (SIPTR) arg;

    PutMsg(port, (struct Message *) packet);
    WaitPort(replyport);
    GetMsg(replyport);

    ret = packet->sp_Pkt.dp_Res1;

    FreeMem(packet, sizeof(struct StandardPacket));
    DeletePort(replyport);

    return ret;
}

static int fill_consize(APTR fd, struct winsize *ws)
{
    struct ConUnit *console_unit = NULL;
    struct InfoData *info_data;
    BPTR action_disk_info_arg;
    void *console = ((struct FileHandle *) BADDR(fd))->fh_Type;

    if(!console)
    {
        return 0;
    }

    info_data = AllocMem(sizeof(*info_data), MEMF_PUBLIC);
    if(info_data == NULL)
    {
        /* no memory, ouch */
        return 0;
    }

    action_disk_info_arg = MKBADDR(info_data);
    if(send_action_packet((struct MsgPort *) console, action_disk_info_arg))
    {
        console_unit = (void *) ((struct IOStdReq *) info_data->id_InUse)->io_Unit;
    }

    /* info_data not required anymore */
    FreeMem(info_data, sizeof(*info_data));

    if (console_unit == NULL)
    {
        return 0;
    }

    ws->ws_row    = (unsigned short) console_unit->cu_YMax+1;
    ws->ws_col    = (unsigned short) console_unit->cu_XMax+1;
    if(console_unit->cu_Window)
    {
        /* does a console always have a window? might be iconified? */
        ws->ws_xpixel = (unsigned short) console_unit->cu_Window->Width;
        ws->ws_ypixel = (unsigned short) console_unit->cu_Window->Height;
    }

    return 1;
}

/*****************************************************************************

    NAME */

#include <sys/ioctl.h>

        int ioctl(

/*  SYNOPSIS */
        int fd,
        int request,
        ...)

/*  FUNCTION
        Control device. Function to manipulate and fetch special device
        parameters.

    INPUTS
        fd      - file descriptor
        request - ioctl request id, containing request type, input or output
                  type and argument size in bytes. Use macros and defines
                  from <sys/ioctl.h>:

                  TIOCGWINSZ - fill in rows, columns, width and height of
                               console window

        ...     - Other arguments for the specified request

    RESULT
        EBADF   - fd is not valid
        EFAULT  - no valid argument
        ENOTTY  - fd is not of required type

    NOTES
        Width and height are the width and height of the intuition window.

    EXAMPLE
        #include <stdio.h>
        #include <unistd.h>
        #include <sys/ioctl.h>

        {
            int ret;
            struct winsize w;
            ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            if(ret)
            {
                printf("ERROR: %d\n", ret);
            }
            else
            {
                printf ("columns: %4d\n", w.ws_col);
                printf ("lines:   %4d\n", w.ws_row);
                printf ("width:   %4d\n", w.ws_xpixel);
                printf ("height:  %4d\n", w.ws_ypixel);
            }
        }

    BUGS
        Only the requests listed above are implemented.

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    va_list args;
    APTR arg = NULL;
    LONG result = 0;
    LONG error;
    fd_type_t owner = __posixc_get_owner(fd);
    fd_type_t posixc_type = __posixc_get_fd_type();

    va_start(args, request);
    arg = va_arg(args, APTR);
    va_end(args);

    if (posixc_type != FD_TYPE_NONE && owner != FD_TYPE_NONE && owner != posixc_type) {
        struct Library *FDBase = NULL;
        struct PosixCIntBase *PosixCBase =
            (struct PosixCIntBase *)__aros_getbase_PosixCBase();

        FDBase = PosixCBase->PosixCFDBase;
        error = FD_Ioctl(fd, request, arg, &result);
        if (error) {
            errno = error;
            return error;
        }
        return result;
    }

    error = __posixc_ioctl(fd, request, arg, &result);

    if (error) {
        if (request != TIOCGWINSZ)
            errno = ENOSYS;
        else
            errno = error;
        return error;
    }

    return result;
}

LONG __posixc_ioctl(int fd, ULONG request, APTR arg, LONG *out_result)
{
    struct winsize *ws;
    fdesc *desc;

    if (!out_result)
        return EINVAL;

    if (request != TIOCGWINSZ)
    {
        /* FIXME: Implement missing ioctl() parameters */
        AROS_FUNCTION_NOT_IMPLEMENTED("posixc");
        return EFAULT;
    }

    switch (fd)
    {
        case STDIN_FILENO:  desc = BADDR(Input());
           break;
        case STDOUT_FILENO: desc = BADDR(Output());
           break;
        default:
           desc = __getfdesc(fd); /* FIXME: is this correct? why does it not work for STDOUT/STDIN? */
    }

    if (!desc || !IsInteractive(desc))
        return EBADF;

    ws = (struct winsize *)arg;
    if (!ws || !fill_consize(desc, ws))
        return EFAULT;

    *out_result = 0;
    return 0;
}
