/*
    Copyright © 1995-2020, The AROS Development Team. All rights reserved.
    $Id$
*/

#ifndef _VMBusHV_H
#define _VMBusHV_H

#include <aros/debug.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/nodes.h>
#include <exec/lists.h>

#include <dos/bptr.h>

#include <oop/oop.h>

#include <aros/arossupportbase.h>
#include <exec/execbase.h>

#include <libraries/acpica.h>

#include LC_LIBDEFS_FILE

struct vmbushv_staticdata
{
    struct Library              *OOPBase;
    struct Library              *utilityBase;
    APTR                        kernelBase;

    OOP_AttrBase                hiddPCIDriverAB;
    OOP_AttrBase                hiddAB;

    OOP_AttrBase                hidd_PCIDeviceAB;

    OOP_MethodID                hidd_PCIDeviceMB;

    OOP_Class                   *vmbushvDriverClass;
    OOP_Class                   *vmbushvDeviceClass;
};

#undef HiddPCIDriverAttrBase
#undef HiddAttrBase
#undef HiddPCIDeviceAttrBase

#undef HiddPCIDeviceBase

#define HiddPCIDriverAttrBase   (PSD(cl)->hiddPCIDriverAB)
#define HiddAttrBase            (PSD(cl)->hiddAB)
#define HiddPCIDeviceAttrBase   (PSD(cl)->hidd_PCIDeviceAB)

#define HiddPCIDeviceBase       (PSD(cl)->hidd_PCIDeviceMB)

#define KernelBase              (PSD(cl)->kernelBase)
#define UtilityBase             (PSD(cl)->utilityBase)
#define OOPBase                 (PSD(cl)->OOPBase)

struct VMBusHVBase
{
    struct Library              LibNode;
    struct vmbushv_staticdata     psd;
};

#define BASE(lib)               ((struct VMBusHVBase*)(lib))
#define PSD(cl)                 (&((struct VMBusHVBase*)cl->UserData)->psd)

#define _psd                    PSD(cl)

struct VMBusHVBusData
{
    APTR                        vbhv_void;
};

struct VMBusHVDeviceData
{
    APTR                        vbdhv_void;
};

#endif /* _VMBusHV_H */
