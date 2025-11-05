/*
    Low level helper routines for scsi.device. These functions translate the
    high level block operations into SCSI command descriptor blocks and submit
    them to the backend host adapter.
*/

#include <aros/debug.h>

#include <proto/exec.h>

#include <string.h>

#include <devices/scsidisk.h>

#include "scsi.h"
#include "scsi_bus.h"

#define DEFAULT_COMMAND_TIMEOUT   30000

static BYTE scsi_MapStatus(struct scsi_Unit *unit, struct SCSI_Command *cmd)
{
    switch (cmd->status)
    {
        case SCSI_STATUS_GOOD:
        case SCSI_STATUS_INTERMEDIATE:
            return 0;
        case SCSI_STATUS_CHECK_CONDITION:
            if (cmd->senseLength >= 14)
            {
                unit->su_SenseKey = cmd->sense[2] & 0x0f;
                unit->su_ASC      = cmd->sense[12];
                unit->su_ASCQ     = cmd->sense[13];
            }
            return HFERR_BadStatus;
        case SCSI_STATUS_BUSY:
            return HFERR_SelTimeout;
        default:
            return HFERR_Phase;
    }
}

BYTE scsi_PerformCommand(struct scsi_Unit *unit, struct SCSI_Command *cmd)
{
    struct scsi_Bus *bus = unit->su_Bus;

    if (!bus)
        return HFERR_SelfUnit;

    cmd->target = unit->su_Target;
    cmd->lun    = unit->su_Lun;
    if (!cmd->timeoutMS)
        cmd->timeoutMS = DEFAULT_COMMAND_TIMEOUT;
    if (!(cmd->flags & SCSI_CF_AUTOSENSE))
        cmd->flags |= SCSI_CF_AUTOSENSE;
    cmd->actualLength = 0;
    unit->su_SenseKey = 0;
    unit->su_ASC = 0;
    unit->su_ASCQ = 0;

    if (!SCSI_BusQueueCommand(bus, cmd))
        return HFERR_Phase;

    return scsi_MapStatus(unit, cmd);
}

BYTE scsi_Inquiry(struct scsi_Unit *unit, UBYTE page, APTR buffer, ULONG length)
{
    struct SCSI_Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdb[0] = SCSI_INQUIRY;
    cmd.cdb[1] = page ? 0x01 : 0x00;
    cmd.cdb[2] = page;
    cmd.cdb[4] = (UBYTE)length;
    cmd.cdbLength = 6;
    cmd.direction = SCSI_DATA_IN;
    cmd.data = buffer;
    cmd.dataLength = length;

    return scsi_PerformCommand(unit, &cmd);
}

BYTE scsi_ReadCapacity(struct scsi_Unit *unit, UBYTE serviceAction, APTR buffer, ULONG length)
{
    struct SCSI_Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    if (serviceAction == 0x10)
    {
        cmd.cdb[0] = SCSI_SERVICE_ACTION_IN_16;
        cmd.cdb[1] = serviceAction;
        cmd.cdb[10] = (length >> 24) & 0xff;
        cmd.cdb[11] = (length >> 16) & 0xff;
        cmd.cdb[12] = (length >> 8) & 0xff;
        cmd.cdb[13] = length & 0xff;
        cmd.cdbLength = 16;
    }
    else
    {
        cmd.cdb[0] = SCSI_READCAPACITY;
        cmd.cdbLength = 10;
    }
    cmd.direction = SCSI_DATA_IN;
    cmd.data = buffer;
    cmd.dataLength = length;

    return scsi_PerformCommand(unit, &cmd);
}

BYTE scsi_TestUnitReady(struct scsi_Unit *unit)
{
    struct SCSI_Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdb[0] = SCSI_TESTUNITREADY;
    cmd.cdbLength = 6;
    cmd.direction = SCSI_DATA_NONE;

    return scsi_PerformCommand(unit, &cmd);
}

BYTE scsi_RequestSense(struct scsi_Unit *unit, APTR buffer, ULONG length)
{
    struct SCSI_Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdb[0] = SCSI_REQUESTSENSE;
    cmd.cdb[4] = (UBYTE)length;
    cmd.cdbLength = 6;
    cmd.direction = SCSI_DATA_IN;
    cmd.data = buffer;
    cmd.dataLength = length;

    return scsi_PerformCommand(unit, &cmd);
}

static BYTE scsi_CommandRW(struct scsi_Unit *unit, UQUAD block, ULONG count,
                           APTR buffer, ULONG *actual, BOOL write, BOOL use16)
{
    struct SCSI_Command cmd;
    ULONG transfer = count * unit->su_BlockSize;
    BYTE err;

    memset(&cmd, 0, sizeof(cmd));
    if (use16)
    {
        cmd.cdb[0] = write ? SCSI_WRITE16 : SCSI_READ16;
        cmd.cdb[1] = 0;
        cmd.cdb[2] = (block >> 56) & 0xff;
        cmd.cdb[3] = (block >> 48) & 0xff;
        cmd.cdb[4] = (block >> 40) & 0xff;
        cmd.cdb[5] = (block >> 32) & 0xff;
        cmd.cdb[6] = (block >> 24) & 0xff;
        cmd.cdb[7] = (block >> 16) & 0xff;
        cmd.cdb[8] = (block >> 8) & 0xff;
        cmd.cdb[9] = block & 0xff;
        cmd.cdb[10] = (count >> 24) & 0xff;
        cmd.cdb[11] = (count >> 16) & 0xff;
        cmd.cdb[12] = (count >> 8) & 0xff;
        cmd.cdb[13] = count & 0xff;
        cmd.cdbLength = 16;
    }
    else
    {
        cmd.cdb[0] = write ? SCSI_WRITE10 : SCSI_READ10;
        cmd.cdb[2] = (block >> 24) & 0xff;
        cmd.cdb[3] = (block >> 16) & 0xff;
        cmd.cdb[4] = (block >> 8) & 0xff;
        cmd.cdb[5] = block & 0xff;
        cmd.cdb[7] = (count >> 8) & 0xff;
        cmd.cdb[8] = count & 0xff;
        cmd.cdbLength = 10;
    }

    cmd.direction = write ? SCSI_DATA_OUT : SCSI_DATA_IN;
    cmd.data = buffer;
    cmd.dataLength = transfer;

    err = scsi_PerformCommand(unit, &cmd);
    if (!err && actual)
        *actual = cmd.actualLength ? cmd.actualLength : transfer;

    return err;
}

static BYTE scsi_SyncCache(struct scsi_Unit *unit)
{
    struct SCSI_Command cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdb[0] = SCSI_SYNCHRONIZE_CACHE;
    cmd.cdbLength = 10;
    cmd.direction = SCSI_DATA_NONE;

    return scsi_PerformCommand(unit, &cmd);
}

static BYTE scsi_ReadBlocks32(struct scsi_Unit *unit, ULONG block, ULONG count,
                              APTR buffer, ULONG *actual)
{
    return scsi_CommandRW(unit, block, count, buffer, actual, FALSE, FALSE);
}

static BYTE scsi_WriteBlocks32(struct scsi_Unit *unit, ULONG block, ULONG count,
                               APTR buffer, ULONG *actual)
{
    return scsi_CommandRW(unit, block, count, buffer, actual, TRUE, FALSE);
}

static BYTE scsi_ReadBlocks64(struct scsi_Unit *unit, UQUAD block, ULONG count,
                              APTR buffer, ULONG *actual)
{
    return scsi_CommandRW(unit, block, count, buffer, actual, FALSE, TRUE);
}

static BYTE scsi_WriteBlocks64(struct scsi_Unit *unit, UQUAD block, ULONG count,
                               APTR buffer, ULONG *actual)
{
    return scsi_CommandRW(unit, block, count, buffer, actual, TRUE, TRUE);
}

void scsi_init_unit(struct scsi_Bus *bus, struct scsi_Unit *unit, UBYTE target, UBYTE lun)
{
    unit->su_Bus = bus;
    unit->su_Target = target;
    unit->su_Lun = lun;
    unit->su_UnitNum = (bus->sb_BusNum << 8) | (target << 4) | lun;
    unit->su_Flags = 0;
    unit->su_InternalFlags = 0;
}

BOOL scsi_setup_unit(struct scsi_Bus *bus, struct scsi_Unit *unit)
{
    UBYTE inquiry[96];
    UBYTE capacity[32];
    BYTE err;

    (void)bus;

    err = scsi_Inquiry(unit, 0, inquiry, sizeof(inquiry));
    if (err)
        return FALSE;

    memcpy(unit->su_Inquiry, inquiry, sizeof(inquiry));
    unit->su_DeviceType = inquiry[0] & 0x1f;
    if (inquiry[1] & 0x80)
        unit->su_Flags |= SCSI_UF_REMOVABLE;
    unit->su_Flags |= SCSI_UF_PRESENT;

    err = scsi_ReadCapacity(unit, 0x10, capacity, sizeof(capacity));
    if (err)
    {
        err = scsi_ReadCapacity(unit, 0x00, capacity, 8);
        if (!err)
        {
            ULONG last = (capacity[0] << 24) | (capacity[1] << 16) |
                         (capacity[2] << 8) | capacity[3];
            ULONG blockSize = (capacity[4] << 24) | (capacity[5] << 16) |
                              (capacity[6] << 8) | capacity[7];

            if (blockSize == 0)
                blockSize = 512;

            unit->su_BlockSize = blockSize;
            unit->su_Capacity = (UQUAD)last + 1;
            unit->su_Capacity48 = unit->su_Capacity;
        }
    }
    else
    {
        UQUAD lastBlock = 0;
        ULONG blockSize = 512;

        lastBlock = ((UQUAD)capacity[0] << 56) |
                    ((UQUAD)capacity[1] << 48) |
                    ((UQUAD)capacity[2] << 40) |
                    ((UQUAD)capacity[3] << 32) |
                    ((UQUAD)capacity[4] << 24) |
                    ((UQUAD)capacity[5] << 16) |
                    ((UQUAD)capacity[6] << 8)  |
                    ((UQUAD)capacity[7]);
        blockSize = (capacity[8] << 24) | (capacity[9] << 16) |
                    (capacity[10] << 8) | capacity[11];

        if (blockSize == 0)
            blockSize = 512;

        unit->su_BlockSize = blockSize;
        unit->su_Capacity = lastBlock + 1;
        unit->su_Capacity48 = unit->su_Capacity;
    }

    {
        UBYTE shift = 0;
        ULONG size = unit->su_BlockSize;
        while ((1UL << shift) < size && shift < 31)
            shift++;
        if ((1UL << shift) != size)
            shift = 9;
        unit->su_SectorShift = shift;
    }

    unit->su_XferModes = 0;
    unit->su_UseModes = 0;

    if (unit->su_DeviceType != SCSI_DEVICE_DIRECT_ACCESS)
        unit->su_XferModes |= AF_XFER_PACKET;

    unit->su_UseModes = unit->su_XferModes;

    unit->su_Heads = 1;
    unit->su_Sectors = 1;
    unit->su_Cylinders = (ULONG)(unit->su_Capacity);

    unit->su_Read32 = scsi_ReadBlocks32;
    unit->su_Write32 = scsi_WriteBlocks32;
    unit->su_Read64 = scsi_ReadBlocks64;
    unit->su_Write64 = scsi_WriteBlocks64;
    unit->su_SynchronizeCache = scsi_SyncCache;

    return TRUE;
}

void scsi_InitBus(struct scsi_Bus *bus)
{
    (void)bus;
}

