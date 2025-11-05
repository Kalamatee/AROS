#ifndef _SCSI_H
#define _SCSI_H

/*
    Generic definitions for scsi.device.
*/

#if !defined(__OOP_NOMETHODBASES__)
#define __OOP_NOMETHODBASES__
#endif

#include <exec/devices.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/ports.h>
#include <oop/oop.h>
#include <utility/hooks.h>
#include <utility/utility.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <devices/newstyle.h>
#include <devices/timer.h>
#include <devices/cd.h>
#include <hardware/scsi.h>
#include <hidd/scsi.h>

#define MAX_DEVICEBUSES         4
#define SCSI_MAX_TARGETS        16
#define SCSI_MAX_LUNS           8
#define STACK_SIZE              16384
#define TASK_PRI                10
#define TIMEOUT                 30

struct scsi_Unit;
struct scsi_Bus;

struct scsiBase
{
    struct Device               scsi_Device;
    struct Task                *scsi_Daemon;
    struct MsgPort             *DaemonPort;
    struct MinList              Daemon_ios;
    struct SignalSemaphore      DaemonSem;
    struct Task                *daemonParent;
    int                         scsi__buscount;
    struct SignalSemaphore      DetectionSem;

    UBYTE                       scsi_EnableTagged;
    UBYTE                       scsi_EnableSynchronous;
    UBYTE                       scsi_EnableWide;
    UBYTE                       scsi_Poll;

    APTR                        scsi_MemPool;

    ULONG                       scsi_ItersPer100ns;

    struct Library             *scsi_OOPBase;
    struct Library             *scsi_UtilityBase;
    BPTR                        scsi_SegList;

    OOP_Class                  *scsiClass;
    OOP_Class                  *busClass;
    OOP_Class                  *unitClass;

#if defined(__OOP_NOATTRBASES__)
    OOP_AttrBase                unitAttrBase;
    OOP_AttrBase                hwAttrBase;
    OOP_AttrBase                busAttrBase;
    OOP_AttrBase                sbusAttrBase;
    OOP_AttrBase                sunitAttrBase;
#endif
#if defined(__OOP_NOMETHODBASES__)
    OOP_MethodID                hwMethodBase;
    OOP_MethodID                busMethodBase;
    OOP_MethodID                HiddSCMethodBase;
#endif

    struct List                 scsi_Controllers;
};

#if defined(__OOP_NOATTRBASES__)
#undef HWAttrBase
#undef HiddBusAB
#undef HiddSCSIBusAB
#undef HiddSCSIUnitAB
#undef HiddStorageUnitAB
#define HWAttrBase                      (SCSIBase->hwAttrBase)
#define HiddBusAB                       (SCSIBase->busAttrBase)
#define HiddSCSIBusAB                   (SCSIBase->sbusAttrBase)
#define HiddSCSIUnitAB                  (SCSIBase->unitAttrBase)
#define HiddStorageUnitAB               (SCSIBase->sunitAttrBase)
#endif

#if defined(__OOP_NOMETHODBASES__)
#undef HWBase
#undef HiddSCSIBusBase
#define HWBase                          (SCSIBase->hwMethodBase)
#define HiddSCSIBusBase                 (SCSIBase->busMethodBase)
#define HiddStorageControllerBase       (SCSIBase->HiddSCMethodBase)
#endif

#define OOPBase                         (SCSIBase->scsi_OOPBase)
#define UtilityBase                     (SCSIBase->scsi_UtilityBase)

struct scsi_Controller
{
    struct Node                 sc_Node;
    OOP_Class                  *sc_Class;
    OOP_Object                 *sc_Object;
};

struct scsi_Bus
{
    struct scsiBase            *sb_Base;

    struct SCSI_BusInterface   *sb_Interface;
    void                       *sb_InterfaceData;
    ULONG                       sb_InterfaceSize;
    ULONG                       sb_Features;
    ULONG                       sb_CommandQueueDepth;
    UBYTE                       sb_MaxTargets;
    UBYTE                       sb_MaxLUNs;
    UBYTE                       sb_BusNum;

    struct scsi_Unit           *sb_SelectedUnit;

    struct Task                *sb_Task;
    struct MsgPort             *sb_MsgPort;
    struct IORequest           *sb_Timer;

    struct Interrupt            sb_ResetInt;

    APTR                        sb_CommandPool;

    ULONG                       sb_IntCnt;
};

struct scsi_Command
{
    struct SCSI_Command         cmd;
    UBYTE                       retries;
};

struct scsi_Unit
{
    struct Unit                 su_Unit;
    struct scsi_Bus            *su_Bus;
    struct IOStdReq            *DaemonReq;

    UBYTE                       su_Target;
    UBYTE                       su_Lun;
    UBYTE                       su_DeviceType;
    UBYTE                       su_Flags;
    UBYTE                       su_SenseKey;
    UBYTE                       su_ASC;
    UBYTE                       su_ASCQ;
    UBYTE                       su_SectorShift;

    ULONG                       su_BlockSize;
    UQUAD                       su_Capacity;
    UQUAD                       su_Capacity48;
    ULONG                       su_XferModes;
    ULONG                       su_UseModes;
    ULONG                       su_ChangeNum;
    ULONG                       su_Cylinders;
    UBYTE                       su_Heads;
    UBYTE                       su_Sectors;

    UBYTE                       su_Inquiry[96];
    struct DriveIdent          *su_Drive;

    BYTE                      (*su_Read32)(struct scsi_Unit *, ULONG, ULONG, APTR, ULONG *);
    BYTE                      (*su_Write32)(struct scsi_Unit *, ULONG, ULONG, APTR, ULONG *);
    BYTE                      (*su_Read64)(struct scsi_Unit *, UQUAD, ULONG, APTR, ULONG *);
    BYTE                      (*su_Write64)(struct scsi_Unit *, UQUAD, ULONG, APTR, ULONG *);
    BYTE                      (*su_SynchronizeCache)(struct scsi_Unit *);
    BYTE                      (*su_DirectSCSI)(struct scsi_Unit *, struct SCSICmd *);

    ULONG                       su_UnitNum;
    ULONG                       su_InternalFlags;

    struct Interrupt           *su_RemoveInt;
    struct List                 su_SoftList;
};

#define SCSI_UNIT_PRESENT       (1 << 0)
#define SCSI_UNIT_REMOVABLE     (1 << 1)
#define SCSI_UNIT_TAGGED        (1 << 2)
#define SCSI_UNIT_SYNCHRONOUS   (1 << 3)

#define AF_DiscPresent          SCSI_UF_PRESENT
#define AF_Removable            SCSI_UF_REMOVABLE
#define AF_DiscChanged          SCSI_UF_CHANGED
#define AF_DMA                  0
#define AF_CHSOnly              0
#define AF_XFER_PACKET          (1 << 0)

#define su_DevType              su_DeviceType

struct DriveIdent
{
   UWORD       id_General;
   UWORD       id_OldCylinders;
   UWORD       id_SpecificConfig;
   UWORD       id_OldHeads;
   UWORD       pad1[2];
   UWORD       id_OldSectors;
   UWORD       pad2[3];
   UBYTE       id_SerialNumber[20];
   UWORD       pad3;
   ULONG       id_BufSize;
   UBYTE       id_FirmwareRev[8];
   UBYTE       id_Model[40];
   UWORD       id_RWMultipleSize;
   union {
      UWORD    id_io32;
      UWORD    id_Trusted;
   };
   UWORD       id_Capabilities;
   UWORD       id_OldCaps;
   UWORD       id_OldPIO;
   UWORD       id_OldDMA;
   UWORD       id_ConfigAvailable;
   UWORD       id_OldLCylinders;
   UWORD       id_OldLHeads;
   UWORD       id_OldLSectors;
   UWORD       pad6[2];
   UWORD       id_RWMultipleTrans;
   ULONG       id_LBASectors;
   UWORD       id_DMADir;
   UWORD       id_MWDMASupport;
   UWORD       id_PIOSupport;
   UWORD       id_MWDMA_MinCycleTime;
   UWORD       id_MWDMA_DefCycleTime;
   UWORD       id_PIO_MinCycleTime;
   UWORD       id_PIO_MinCycleTimeIORDY;
   UWORD       pad8[6];
   UWORD       id_QueueDepth;
   UWORD       pad9[4];
   UWORD       id_SCSIVersion;
   UWORD       id_SCSIRevision;
   UWORD       id_Commands1;
   UWORD       id_Commands2;
   UWORD       id_Commands3;
   UWORD       id_Commands4;
   UWORD       id_Commands5;
   UWORD       id_Commands6;
   UWORD       id_UDMASupport;
   UWORD       id_SecurityEraseTime;
   UWORD       id_ESecurityEraseTime;
   UWORD       id_CurrentAdvPowerMode;
   UWORD       id_MasterPwdRevision;
   UWORD       id_HWResetResult;
   UWORD       id_AcousticManagement;
   UWORD       id_StreamMinimunReqSize;
   UWORD       id_StreamingTimeDMA;
   UWORD       id_StreamingLatency;
   ULONG       id_StreamingGranularity;
   UQUAD       id_LBA48Sectors;
   UWORD       id_StreamingTimePIO;
   UWORD       pad10;
   UWORD       id_PhysSectorSize;
   UWORD       pad11;
   UQUAD       id_UniqueIDi[2];
   UWORD       pad12;
   ULONG       id_WordsPerLogicalSector;
   UWORD       pad13[8];
   UWORD       id_RemMediaStatusNotificationFeatures;
   UWORD       id_SecurityStatus;
   UWORD       pad14[40];
   UWORD       id_DSManagement;
   UWORD       pad15[86];
} __attribute__((packed));

#define Unit(io) ((struct scsi_Unit *)(io)->io_Unit)
#define IOStdReq(io) ((struct IOStdReq *)io)

BOOL Hidd_SCSIBus_Start(OOP_Object *o, struct scsiBase *SCSIBase);
AROS_UFP3(BOOL, Hidd_SCSIBus_Open,
          AROS_UFPA(struct Hook *, h, A0),
          AROS_UFPA(OOP_Object *, obj, A2),
          AROS_UFPA(IPTR, reqUnit, A1));

void scsi_InitBus(struct scsi_Bus *);
BOOL scsi_setup_unit(struct scsi_Bus *bus, struct scsi_Unit *unit);
void scsi_init_unit(struct scsi_Bus *bus, struct scsi_Unit *unit, UBYTE target, UBYTE lun);

BOOL scsi_RegisterVolume(ULONG StartCyl, ULONG EndCyl, struct scsi_Unit *unit);
void BusTaskCode(struct scsi_Bus *bus, struct scsiBase *SCSIBase);
void DaemonCode(struct scsiBase *LIBBASE);

BYTE scsi_PerformCommand(struct scsi_Unit *unit, struct SCSI_Command *cmd);
BYTE scsi_Inquiry(struct scsi_Unit *unit, UBYTE page, APTR buffer, ULONG length);
BYTE scsi_ReadCapacity(struct scsi_Unit *unit, UBYTE serviceAction, APTR buffer, ULONG length);
BYTE scsi_TestUnitReady(struct scsi_Unit *unit);
BYTE scsi_RequestSense(struct scsi_Unit *unit, APTR buffer, ULONG length);

#endif // _SCSI_H


