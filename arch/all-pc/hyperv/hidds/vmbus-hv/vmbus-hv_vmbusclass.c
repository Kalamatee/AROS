/*
    Copyright © 2004-2020, The AROS Development Team. All rights reserved.
    $Id$

    Desc: PCI direct bus driver, for i386/x86_64 native.
    Lang: English
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/oop.h>
#include <proto/acpica.h>

#include <aros/symbolsets.h>
#include <hidd/hidd.h>
#include <hidd/pci.h>
#include <hardware/pci.h>
#include <oop/oop.h>
#include <utility/tagitem.h>

#include <acpica/acnames.h>
#include <acpica/accommon.h>

#include <string.h>

#include "vmbus-hv_intern.h"

#define DMMIO(x)

/*
 * N.B. Do not move/remove/refactor the following variable unless you fully
 * understand the implications. It must be an explicit global variable for the
 * following reasons:
 *  - Linking with the linklib: acpica.library is a stack-call library, which
 *    means that its functions are always called through a stub linklib.
 *    Linking with the linklib will fail if ACPICABase is not a global variable.
 *  - acpica.library is optional: this class must still be able to run if
 *    acpica.library is unavailable. If ACPICABase is not defined as a global
 *    variable, the autoinit system will create one. However, in that case the
 *    autoinit system will also take over the opening of the library, and not
 *    allow this class to be loaded if the library isn't found.
 */
struct Library *ACPICABase;

const char vmbushvHW[] = "Hyper-V VMBus";

struct acpiHostBridge
{
    struct Node         ahb_Node;
    struct MinList      ahb_irqRoutingTable;
};

/*
    We overload the New method in order to introduce the HardwareName attributes.
*/
OOP_Object *VMBusHV__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
#if (0)
    struct MinList *routingtable = (struct MinList *)GetTagData(aHidd_PCIVMBus_IRQRoutingTable, 0, msg->attrList);
#endif
    struct pRoot_New vmbMsg;
    struct TagItem vmbTags[] =
    {
        { aHidd_Name,           (IPTR)"vmbus-hv.hidd"			        },
        { aHidd_HardwareName,   (IPTR)vmbushvHW                                 },
        { TAG_DONE,             0 					        }
    };
    IPTR mmbase = 0;
    OOP_Object *busObj;

    D(bug("[VMBusHV:VMBus] %s()\n", __func__);)

    vmbMsg.mID      = msg->mID;
    vmbMsg.attrList = vmbTags;

    if (msg->attrList)
    {
        vmbTags[1].ti_Tag  = TAG_MORE;
        vmbTags[1].ti_Data = (IPTR)msg->attrList;
    }

    busObj = (OOP_Object *)OOP_DoSuperMethod(cl, o, &vmbMsg.mID);
    if (busObj)
    {
        struct VMBusHVBusData *data = OOP_INST_DATA(cl, busObj);
        D(bug("[VMBusHV:VMBus] %s: VMBus Object created @ 0x%p\n", __func__, busObj);)
    }
    D(bug("[VMBusHV:VMBus] %s: returning 0x%p\n", __func__, busObj);)

    return busObj;
}

void VMBusHV__Root__Get(OOP_Class *cl, OOP_Object *o, struct pRoot_Get *msg)
{
    struct VMBusHVBusData *data = OOP_INST_DATA(cl, o);
    struct vmbushv_staticdata *psd = PSD(cl);
    ULONG idx;
    BOOL handled = FALSE;

    if (!handled)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    }
}

#undef _psd
#undef UtilityBase
#define UtilityBase (psd->utilityBase)

#undef OOPBase
#undef HiddPCIVMBusAttrBase
#define HiddPCIVMBusAttrBase (_psd->hiddPCIVMBusAB)

static int VMBusHV_InitClass(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;
    struct vmbushv_staticdata *_psd = &LIBBASE->psd;

    D(bug("[VMBusHV:VMBus] %s()\n", __func__));

    _psd->hiddAB = OOP_ObtainAttrBase(IID_Hidd);
    _psd->hidd_PCIDeviceAB = OOP_ObtainAttrBase(IID_Hidd_PCIDevice);

    if (_psd->hiddAB == 0 ||
        _psd->hidd_PCIDeviceAB == 0)
    {
        bug("[VMBusHV:VMBus] %s: ObtainAttrBases failed\n", __func__);
        return FALSE;
    }

    /* Open ACPI and cache the pointer to the MCFG table */
    ACPICABase = OpenLibrary("acpica.library", 0);
    if (ACPICABase)
    {
    }

    D(bug("[VMBusHV:VMBus] %s: done\n", __func__));

    return TRUE;
}

static int VMBusHV_ExpungeClass(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;

    D(bug("[VMBusHV:VMBus] %s()\n", __func__));

    OOP_ReleaseAttrBase(IID_Hidd_PCIDevice);

    OOP_ReleaseAttrBase(IID_Hidd);

    if (ACPICABase)
        CloseLibrary(ACPICABase);

    return TRUE;
}

ADD2INITLIB(VMBusHV_InitClass, 10)
ADD2EXPUNGELIB(VMBusHV_ExpungeClass, 0)

