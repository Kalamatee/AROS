#ifndef DEVICES_SCSICMDS_H
#define DEVICES_SCSICMDS_H

/*
    Copyright  1995-2024, The AROS Development Team. All rights reserved.

    Desc: SCSI commands
    Lang: english
*/

#define SCSI_TESTUNITREADY      0x00
#define SCSI_REZEROUNIT         0x01
#define SCSI_REQUESTSENSE       0x03
#define SCSI_FORMATUNIT         0x04
#define SCSI_READ6              0x08
#define SCSI_WRITE6             0x0A
#define SCSI_SEEK6              0x0B
#define SCSI_INQUIRY            0x12
#define SCSI_VERIFY6            0x13
#define SCSI_MODESELECT6        0x15
#define SCSI_RESERVE6           0x16
#define SCSI_RELEASE6           0x17
#define SCSI_MODESENSE6         0x1A
#define SCSI_STARTSTOP          0x1B
#define SCSI_SENDDIAGNOSTIC     0x1D
#define SCSI_READCAPACITY       0x25
#define SCSI_READ10             0x28
#define SCSI_WRITE10            0x2A
#define SCSI_SEEK10             0x2B
#define SCSI_VERIFY10           0x2F
#define SCSI_SYNCHRONIZE_CACHE  0x35
#define SCSI_MODESELECT10       0x55
#define SCSI_RESERVE10          0x56
#define SCSI_RELEASE10          0x57
#define SCSI_MODESENSE10        0x5A
#define SCSI_READ12             0xA8
#define SCSI_WRITE12            0xAA
#define SCSI_VERIFY12           0xAF
#define SCSI_READ16             0x88
#define SCSI_WRITE16            0x8A
#define SCSI_VERIFY16           0x8F
#define SCSI_SERVICE_ACTION_IN_16 0x9E

#endif /* DEVICES_SCSICMDS_H */
