/*
    Copyright (C) 2019-2024, The AROS Development Team. All rights reserved.

    Generic SCSI bus base class. This class provides helper logic that keeps a
    cached pointer to the backend host interface so the frontend can call into
    the driver without going through OOP message dispatch for every command.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#define __NOLIBBASE__

#include <proto/oop.h>
#include <proto/utility.h>

#include <hidd/storage.h>
#include <hidd/bus.h>
#include <hidd/scsi.h>
#include <oop/oop.h>
#include <utility/tagitem.h>
#include <string.h>

#include "scsi.h"
#include "scsi_bus.h"

static void CopyInterface(struct SCSI_BusInterface *dst, const struct SCSI_BusInterface *src)
{
    if (!src)
    {
        dst->queue      = NULL;
        dst->reset      = NULL;
        dst->set_target = NULL;
        dst->poll       = NULL;
        return;
    }

    dst->queue      = src->queue;
    dst->reset      = src->reset;
    dst->set_target = src->set_target;
    dst->poll       = src->poll;
}

OOP_Object *SCSIBus__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct scsiBase *SCSIBase = cl->UserData;
    struct scsi_Bus *data;
    struct TagItem *tag;
    struct TagItem *state;
    const struct SCSI_BusInterface *vectors = NULL;
    ULONG interfaceSize = 0;

    D(bug("[SCSI:Bus] %s()\n", __func__));

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (!o)
        return NULL;

    data = OOP_INST_DATA(cl, o);
    memset(data, 0, sizeof(*data));
    data->sb_Base = SCSIBase;

    state = msg->attrList;
    while ((tag = NextTagItem(&state)))
    {
        switch (tag->ti_Tag)
        {
            case aoHidd_SCSIBus_MaxTargets:
                data->sb_MaxTargets = (UBYTE)tag->ti_Data;
                break;
            case aoHidd_SCSIBus_MaxLUNs:
                data->sb_MaxLUNs = (UBYTE)tag->ti_Data;
                break;
            case aoHidd_SCSIBus_Features:
                data->sb_Features = tag->ti_Data;
                break;
            case aoHidd_SCSIBus_CommandQueueDepth:
                data->sb_CommandQueueDepth = tag->ti_Data;
                break;
            case aoHidd_SCSIBus_InterfaceDataSize:
                interfaceSize = tag->ti_Data;
                break;
            case aoHidd_SCSIBus_InterfaceVectors:
                vectors = (const struct SCSI_BusInterface *)tag->ti_Data;
                break;
            default:
                break;
        }
    }

    if (!data->sb_MaxTargets)
        data->sb_MaxTargets = SCSI_MAX_TARGETS;
    if (!data->sb_MaxLUNs)
        data->sb_MaxLUNs = SCSI_MAX_LUNS;

    if (vectors || interfaceSize)
    {
        ULONG allocSize = sizeof(struct SCSI_BusInterface) + interfaceSize;
        struct SCSI_BusInterface *iface = AllocMem(allocSize, MEMF_PUBLIC | MEMF_CLEAR);
        if (!iface)
        {
            OOP_MethodID dispose = msg->mID - moRoot_New + moRoot_Dispose;
            OOP_DoSuperMethod(cl, o, (OOP_Msg)&dispose);
            return NULL;
        }

        CopyInterface(iface, vectors);
        data->sb_Interface = iface;
        data->sb_InterfaceData = iface + 1;
        data->sb_InterfaceSize = allocSize;
    }

    return o;
}

VOID SCSIBus__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    struct scsi_Bus *data = OOP_INST_DATA(cl, o);

    if (data->sb_Interface)
    {
        FreeMem(data->sb_Interface, data->sb_InterfaceSize ? data->sb_InterfaceSize : sizeof(struct SCSI_BusInterface));
        data->sb_Interface = NULL;
        data->sb_InterfaceData = NULL;
        data->sb_InterfaceSize = 0;
    }

    OOP_DoSuperMethod(cl, o, msg);
}

void SCSIBus__Root__Get(OOP_Class *cl, OOP_Object *o, struct pRoot_Get *msg)
{
    struct scsi_Bus *data = OOP_INST_DATA(cl, o);

    switch (msg->attrID)
    {
        case aoHidd_SCSIBus_MaxTargets:
            *msg->storage = data->sb_MaxTargets;
            return;
        case aoHidd_SCSIBus_MaxLUNs:
            *msg->storage = data->sb_MaxLUNs;
            return;
        case aoHidd_SCSIBus_Features:
            *msg->storage = data->sb_Features;
            return;
        case aoHidd_SCSIBus_CommandQueueDepth:
            *msg->storage = data->sb_CommandQueueDepth;
            return;
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

APTR SCSIBus__Hidd_SCSIBus__GetHostInterface(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    struct scsi_Bus *data = OOP_INST_DATA(cl, o);
    return data->sb_InterfaceData;
}

BOOL Hidd_SCSIBus_Start(OOP_Object *o, struct scsiBase *SCSIBase)
{
    struct scsi_Bus *bus = OOP_INST_DATA(SCSIBase->busClass, o);

    if (!bus->sb_Interface)
        return FALSE;

    /* Bus is ready for discovery */
    scsi_InitBus(bus);
    return TRUE;
}

AROS_UFH3(BOOL, SCSIBus__Hidd_Bus__Open,
          AROS_UFHA(struct Hook *, hook, A0),
          AROS_UFHA(OOP_Object *, obj, A2),
          AROS_UFHA(IPTR, unit, A1))
{
    AROS_USERFUNC_INIT
    return TRUE;
    AROS_USERFUNC_EXIT
}

