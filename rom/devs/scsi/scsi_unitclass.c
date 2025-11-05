/*
    Copyright (C) 2019-2024, The AROS Development Team. All rights reserved.

    Private unit class used by scsi.device to represent logical units that are
    discovered on a SCSI bus. The class mostly acts as a container for metadata
    describing the device and exposes a number of read-only attributes to other
    components in the system.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#define __NOLIBBASE__

#include <hidd/storage.h>
#include <hidd/scsi.h>
#include <oop/oop.h>
#include <string.h>

#include "scsi.h"

static BYTE scsi_Unsupported(struct scsi_Unit *unit)
{
    D(bug("[SCSI%02ld] unsupported operation on unit\n", unit->su_UnitNum));
    return CDERR_NOCMD;
}

static BYTE scsi_UnsupportedIO32(struct scsi_Unit *unit, ULONG blk, ULONG len,
                                 APTR buf, ULONG *actual)
{
    (void)blk;
    (void)len;
    (void)buf;
    if (actual)
        *actual = 0;
    return scsi_Unsupported(unit);
}

static BYTE scsi_UnsupportedIO64(struct scsi_Unit *unit, UQUAD blk, ULONG len,
                                 APTR buf, ULONG *actual)
{
    (void)blk;
    (void)len;
    (void)buf;
    if (actual)
        *actual = 0;
    return scsi_Unsupported(unit);
}

static BYTE scsi_UnsupportedSCSI(struct scsi_Unit *unit, struct SCSICmd *cmd)
{
    (void)cmd;
    return scsi_Unsupported(unit);
}

OOP_Object *SCSIUnit__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct scsiBase *SCSIBase = cl->UserData;
    struct scsi_Unit *unit;

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (!o)
        return NULL;

    unit = OOP_INST_DATA(cl, o);
    memset(unit, 0, sizeof(*unit));

    unit->su_Drive = AllocPooled(SCSIBase->scsi_MemPool, sizeof(struct DriveIdent));
    if (!unit->su_Drive)
    {
        OOP_MethodID dispose = msg->mID - moRoot_New + moRoot_Dispose;
        OOP_DoSuperMethod(cl, o, (OOP_Msg)&dispose);
        return NULL;
    }
    memset(unit->su_Drive, 0, sizeof(struct DriveIdent));

    unit->su_Bus = NULL;
    unit->su_BlockSize = 512;
    NEWLIST(&unit->su_SoftList);

    unit->su_Read32          = scsi_UnsupportedIO32;
    unit->su_Write32         = scsi_UnsupportedIO32;
    unit->su_Read64          = scsi_UnsupportedIO64;
    unit->su_Write64         = scsi_UnsupportedIO64;
    unit->su_SynchronizeCache= scsi_Unsupported;
    unit->su_DirectSCSI      = scsi_UnsupportedSCSI;

    return o;
}

VOID SCSIUnit__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    struct scsiBase *SCSIBase = cl->UserData;
    struct scsi_Unit *unit = OOP_INST_DATA(cl, o);

    if (unit->su_Drive)
        FreePooled(SCSIBase->scsi_MemPool, unit->su_Drive, sizeof(struct DriveIdent));

    OOP_DoSuperMethod(cl, o, msg);
}

void SCSIUnit__Root__Get(OOP_Class *cl, OOP_Object *o, struct pRoot_Get *msg)
{
    struct scsi_Unit *unit = OOP_INST_DATA(cl, o);

    switch (msg->attrID)
    {
        case aoHidd_SCSIUnit_TargetID:
            *msg->storage = unit->su_Target;
            return;
        case aoHidd_SCSIUnit_LUN:
            *msg->storage = unit->su_Lun;
            return;
        case aoHidd_SCSIUnit_DeviceType:
            *msg->storage = unit->su_DeviceType;
            return;
        case aoHidd_SCSIUnit_BlockSize:
            *msg->storage = unit->su_BlockSize;
            return;
        case aoHidd_SCSIUnit_Capacity:
            *msg->storage = unit->su_Capacity;
            return;
        case aoHidd_SCSIUnit_Flags:
            *msg->storage = unit->su_Flags;
            return;
        case aoHidd_SCSIUnit_InquiryData:
            *msg->storage = (IPTR)unit->su_Inquiry;
            return;
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

