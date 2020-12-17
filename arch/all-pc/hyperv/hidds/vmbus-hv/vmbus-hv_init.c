/*
    Copyright © 2020, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/oop.h>

#include <aros/symbolsets.h>

#include <exec/execbase.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>

#include <hidd/pci.h>

#include <utility/utility.h>

#include LC_LIBDEFS_FILE
#include "vmbus-hv_intern.h"

#undef OOPBase

static int VMBusHV_Init(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;

    D(
        bug("[VMBusHV] %s()\n", __func__);
        bug("[VMBusHV] %s: OOPBase @ 0x%p\n", __func__, OOPBase);
    )

    LIBBASE->psd.kernelBase = OpenResource("kernel.resource");
    if (!LIBBASE->psd.kernelBase)
        return FALSE;

    D(bug("[VMBusHV] %s: KernelBase @ 0x%p\n", __func__, LIBBASE->psd.kernelBase);)

    LIBBASE->psd.utilityBase = TaggedOpenLibrary(TAGGEDOPEN_UTILITY);
    if (!LIBBASE->psd.utilityBase)
        return FALSE;

    D(bug("[VMBusHV] %s: UtilityBase @ 0x%p\n", __func__, LIBBASE->psd.utilityBase);)

    LIBBASE->psd.hwMethodBase = OOP_GetMethodID(IID_HW, 0);

    D(bug("[VMBusHV] %s: HW MB = %08x\n", __func__, LIBBASE->psd.hwMethodBase);)

    return TRUE;
}

static int VMBusHV_Expunge(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;
    int ok;

    D(bug("[VMBusHV] %s()\n", __func__));

    OOP_Object *pci = OOP_NewObject(NULL, CLID_Hidd_PCI, NULL);
    if (pci)
    {
        struct pHidd_PCI_RemHardwareDriver msg, *pmsg=&msg;

        msg.mID = OOP_GetMethodID(IID_Hidd_PCI, moHidd_PCI_RemHardwareDriver);
        msg.driverClass = LIBBASE->psd.vmbushvBusClass;

        ok = OOP_DoMethod(pci, (OOP_Msg)pmsg);

        OOP_DisposeObject(pci);
    }
    else
    {
        ok = FALSE;
    }

    return ok;
}

ADD2INITLIB(VMBusHV_Init, 0)
ADD2EXPUNGELIB(VMBusHV_Expunge, 0)

ADD2LIBS("bus.hidd", 0, static struct Library *, __pcihidd)
