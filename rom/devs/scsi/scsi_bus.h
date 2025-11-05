#ifndef SCSI_BUS_HELPERS_H
#define SCSI_BUS_HELPERS_H

/*
    Helper inline wrappers for accessing the SCSI host interface.
*/

static inline BOOL SCSI_BusQueueCommand(struct scsi_Bus *bus, struct SCSI_Command *cmd)
{
    if (!bus->sb_Interface || !bus->sb_Interface->queue)
        return FALSE;

    return bus->sb_Interface->queue(bus->sb_InterfaceData, cmd);
}

static inline BOOL SCSI_BusReset(struct scsi_Bus *bus, ULONG flags)
{
    if (!bus->sb_Interface || !bus->sb_Interface->reset)
        return FALSE;

    return bus->sb_Interface->reset(bus->sb_InterfaceData, flags);
}

static inline BOOL SCSI_BusConfigureTarget(struct scsi_Bus *bus, UBYTE target,
                                           const struct SCSI_TargetSettings *settings)
{
    if (!bus->sb_Interface || !bus->sb_Interface->set_target)
        return FALSE;

    return bus->sb_Interface->set_target(bus->sb_InterfaceData, target, settings);
}

static inline VOID SCSI_BusPoll(struct scsi_Bus *bus)
{
    if (bus->sb_Interface && bus->sb_Interface->poll)
        bus->sb_Interface->poll(bus->sb_InterfaceData);
}

#endif /* SCSI_BUS_HELPERS_H */
