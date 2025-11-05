#ifndef HIDD_SCSI_H
#define HIDD_SCSI_H

/*
    Copyright  2013-2024, The AROS Development Team. All rights reserved.

    Desc: Generic SCSI host controller definitions shared between
          the scsi.device frontend and backend host adapters.
    Lang: english
*/

#include <exec/types.h>

#define CLID_Hidd_SCSI           "hidd.scsi"
#define CLID_Hidd_SCSIBus        "hidd.scsi.bus"

#define SCSI_MAX_CDB_LENGTH      16
#define SCSI_MAX_SENSE_LENGTH    32

typedef enum
{
    SCSI_DATA_NONE,
    SCSI_DATA_IN,
    SCSI_DATA_OUT,
    SCSI_DATA_BIDIRECTIONAL
} SCSI_DataDirection;

enum SCSI_CommandFlags
{
    SCSI_CF_AUTOSENSE   = 1 << 0,
    SCSI_CF_TAGGED      = 1 << 1,
    SCSI_CF_POLL        = 1 << 2,
    SCSI_CF_ORDERED     = 1 << 3,
    SCSI_CF_QUIET       = 1 << 4
};

struct SCSI_Command
{
    UBYTE               target;                     /* Target identifier              */
    UBYTE               lun;                        /* Logical unit number            */
    UBYTE               cdb[SCSI_MAX_CDB_LENGTH];   /* Command descriptor block       */
    UBYTE               cdbLength;                  /* Length of the CDB              */
    SCSI_DataDirection  direction;                  /* Expected data transfer         */
    APTR                data;                       /* Data buffer pointer            */
    ULONG               dataLength;                 /* Requested transfer size        */
    ULONG               actualLength;               /* Completed transfer size        */
    ULONG               timeoutMS;                  /* Command timeout in milliseconds*/
    UBYTE               status;                     /* Returned SCSI status byte      */
    UBYTE               sense[SCSI_MAX_SENSE_LENGTH]; /* Sense data buffer            */
    UBYTE               senseLength;                /* Valid sense data length        */
    UBYTE               flags;                      /* SCSI_CommandFlags combination  */
};

enum SCSI_BusFeature
{
    SCSI_BF_PARITY      = 1 << 0,
    SCSI_BF_DMA         = 1 << 1,
    SCSI_BF_SYNC        = 1 << 2,
    SCSI_BF_WIDE        = 1 << 3,
    SCSI_BF_TAGGED      = 1 << 4,
    SCSI_BF_AUTOSENSE   = 1 << 5
};

struct SCSI_TargetSettings
{
    UWORD               transferPeriodNS;           /* Requested synchronous period   */
    UBYTE               maxOffset;                  /* REQ/ACK offset                 */
    UBYTE               busWidth;                   /* 0 = 8-bit, 1 = 16-bit, etc.    */
    UBYTE               flags;                      /* SCSI_TargetFlag combination    */
};

enum SCSI_TargetFlag
{
    SCSI_TF_ALLOW_DISC  = 1 << 0,
    SCSI_TF_TAGGED      = 1 << 1
};

struct SCSI_BusInterface
{
    BOOL  (*queue)(void *obj, struct SCSI_Command *cmd);                     /* Submit and wait for command */
    BOOL  (*reset)(void *obj, ULONG flags);                                  /* Reset bus or target         */
    BOOL  (*set_target)(void *obj, UBYTE target, const struct SCSI_TargetSettings *settings);
    VOID  (*poll)(void *obj);                                                /* Poll outstanding activity   */
};

enum SCSI_ResetFlags
{
    SCSI_RESET_BUS      = 1 << 0,
    SCSI_RESET_DEVICE   = 1 << 1,
    SCSI_RESET_ABORT    = 1 << 2
};

enum SCSI_BusAttribute
{
    aoHidd_SCSIBus_MaxTargets = 0x08000000,
    aoHidd_SCSIBus_MaxLUNs,
    aoHidd_SCSIBus_Features,
    aoHidd_SCSIBus_InterfaceDataSize,
    aoHidd_SCSIBus_InterfaceVectors,
    aoHidd_SCSIBus_CommandQueueDepth,
    aoHidd_SCSIBus_MinTransferPeriod,
    aoHidd_SCSIBus_MaxTransferPeriod
};

enum SCSI_UnitAttribute
{
    aoHidd_SCSIUnit_TargetID = 0x08000100,
    aoHidd_SCSIUnit_LUN,
    aoHidd_SCSIUnit_DeviceType,
    aoHidd_SCSIUnit_BlockSize,
    aoHidd_SCSIUnit_Capacity,
    aoHidd_SCSIUnit_Flags,
    aoHidd_SCSIUnit_InquiryData
};

enum SCSI_UnitFlag
{
    SCSI_UF_REMOVABLE   = 1 << 0,
    SCSI_UF_PRESENT     = 1 << 1,
    SCSI_UF_TAGGED      = 1 << 2,
    SCSI_UF_SYNC        = 1 << 3,
    SCSI_UF_CHANGED     = 1 << 4
};

#endif /* HIDD_SCSI_H */
