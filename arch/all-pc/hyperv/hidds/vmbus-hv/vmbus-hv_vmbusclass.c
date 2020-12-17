/*
    Copyright © 2020, The AROS Development Team. All rights reserved.
    $Id$
    Lang: English
*/

#include <aros/debug.h>

#include <proto/kernel.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/oop.h>

#include <aros/symbolsets.h>
#include <hidd/hidd.h>
#include <hidd/system.h>
#include <oop/oop.h>
#include <utility/tagitem.h>

#include <resources/processor.h>

#include <string.h>

#include "vmbus-hv_intern.h"

#define DMMIO(x)

const char vmbushvHW[] = "Hyper-V VMBus";

struct acpiHostBridge
{
    struct Node         ahb_Node;
    struct MinList      ahb_irqRoutingTable;
};

static void VMBusHV_IntHandler(void *data1, void *data2)
{
    D(bug("[VMBusHV:VMBus] %s()\n", __func__);)
}

OOP_Object *VMBusHV__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct pRoot_New vmbMsg;
    struct TagItem vmbTags[] =
    {
        { aHidd_Name,           (IPTR)"vmbus-hv.hidd"			        },
        { aHidd_HardwareName,   (IPTR)vmbushvHW                                 },
        { TAG_DONE,             0 					        }
    };
    IPTR mmbase = 0;
    OOP_Object *busObj;
    BOOL vmbusok = FALSE;

    D(bug("[VMBusHV:VMBus] %s()\n", __func__);)

    vmbMsg.mID      = msg->mID;
    vmbMsg.attrList = vmbTags;

    if (msg->attrList)
    {
        vmbTags[1].ti_Tag  = TAG_MORE;
        vmbTags[1].ti_Data = (IPTR)msg->attrList;
    }

    if ((busObj = PSD(cl)->hvbusObj) == NULL)
    {
        busObj = (OOP_Object *)OOP_DoSuperMethod(cl, o, &vmbMsg.mID);
        if (busObj)
        {
            struct VMBusHVBusData *data = OOP_INST_DATA(cl, busObj);

            D(bug("[VMBusHV:VMBus] %s: VMBus Object created @ 0x%p\n", __func__, busObj);)

            if ((data->vbhv_HCAlloc = AllocMem(PAGE_SIZE + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR)) != NULL)
            {
                hyperv_vmbus_hypercall_t hcBuff;
                APTR ssp = NULL;
                ULONG eax, edx;

                data->vbhv_HCPage = (APTR)(((IPTR)data->vbhv_HCAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));
                D(bug("[VMBusHV:VMBus] %s: Hypercall Buffer @ 0x%p (alloc @ 0x%p)\n", __func__, data->vbhv_HCPage, data->vbhv_HCAlloc);)

                hcBuff.raw = 0;
                if ((ssp = SuperState()) != NULL)
                {
                    /* set the Guest OS ID - just set the open source bit for now ... */
#define HYPERV_GUESTID_AROS	((UQUAD)0x8000 << 48)
                    wrmsr(MSR_HYPERV_GUESTOS_ID, HYPERV_GUESTID_AROS);

                    D(bug("[VMBusHV:VMBus] %s: GuestID set (%08x%08x)\n", __func__, (HYPERV_GUESTID_AROS >> 32) & 0xFFFFFFFF, HYPERV_GUESTID_AROS & 0xFFFFFFFF);)

                    eax = edx = 0;
                    rdmsr(MSR_HYPERV_HYPERCALL, &eax, &edx);
                    hcBuff.raw = ((UQUAD)edx << 32) | eax;

                    D(bug("[VMBusHV:VMBus] %s: Initial Hypercall MSR value = %08x%08x\n", __func__, edx, eax);)

                    hcBuff.bits.enable = 1;
                    hcBuff.bits.guestpa = ((IPTR)data->vbhv_HCPage >> PAGE_SHIFT);
                    D(bug("[VMBusHV:VMBus] %s: attempting to set to %08x%08x\n", __func__, (hcBuff.raw >> 32) & 0xFFFFFFFF, hcBuff.raw & 0xFFFFFFFF);)
                    wrmsr(MSR_HYPERV_HYPERCALL, hcBuff.raw);

                    /* make sure hypercall was enabled ... */
                    eax = edx = 0;
                    rdmsr(MSR_HYPERV_HYPERCALL, &eax, &edx);
                    D(bug("[VMBusHV:VMBus] %s: post-config Hypercall MSR value = %08x%08x\n", __func__, edx, eax);)
                    hcBuff.raw = ((UQUAD)edx << 32) | eax;
                    UserState(ssp);
                }
                if (hcBuff.bits.enable)
                {
                    ULONG apicBusIRQ;

                    D(bug("[VMBusHV:VMBus] %s: HV Hypercall succesfully configured\n", __func__);)

#if (0)
                    apicBusIRQ = KrnAllocIRQ(IRQTYPE_APIC, 1);
#else
                    apicBusIRQ = 0xB0; // WARN: Test
#endif
                    if (apicBusIRQ != (ULONG)-1)
                    {
                        D(bug("[VMBusHV:VMBus] %s: Allocated IRQ #%u\n", __func__, apicBusIRQ);)
                        KrnAddIRQHandler(apicBusIRQ, VMBusHV_IntHandler, NULL, NULL);

                        data->vbhv_SynICMsgAlloc = AllocMem(PAGE_SIZE + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
                        data->vbhv_SynICMsgPage = (APTR)(((IPTR)data->vbhv_HCAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));
                        D(bug("[VMBusHV:VMBus] %s: SynIC Msg Page @ 0x%p (alloc @ 0x%p)\n", __func__, data->vbhv_SynICMsgPage, data->vbhv_SynICMsgAlloc);)
                        data->vbhv_SynICEventAlloc = AllocMem(PAGE_SIZE + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
                        data->vbhv_SynICEventPage = (APTR)(((IPTR)data->vbhv_HCAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));
                        D(bug("[VMBusHV:VMBus] %s: SynIC Event Page @ 0x%p (alloc @ 0x%p)\n", __func__, data->vbhv_SynICEventPage, data->vbhv_SynICEventAlloc);)

                        if ((ssp = SuperState()) != NULL)
                        {
                            hv_vmbus_synic_simp         simp;
                            hv_vmbus_synic_siefp	siefp;
                            hv_vmbus_synic_scontrol     sctrl;
                            hv_vmbus_synic_sint         shared_sint;

                            D(bug("[VMBusHV:VMBus] %s: Configuring SynIC Msg Page ..\n", __func__);)
                            /* Configure the Synic's message page */
                            eax = edx = 0;
                            rdmsr(MSR_HYPERV_SIMP, &eax, &edx);
                            simp.raw = ((UQUAD)edx << 32) | eax;
                            simp.u.simp_enabled = 1;
                            simp.u.base_simp_gpa = ((IPTR)data->vbhv_SynICMsgPage >> PAGE_SHIFT);
                            wrmsr(MSR_HYPERV_SIMP, simp.raw);
                            eax = edx = 0;
                            rdmsr(MSR_HYPERV_SIMP, &eax, &edx);
                            simp.raw = ((UQUAD)edx << 32) | eax;
            
                            if (simp.u.simp_enabled)
                            {
                                D(bug("[VMBusHV:VMBus] %s: Configuring SynIC Event Page ..\n", __func__);)
                                /* Configure the Synic's event page */
                                eax = edx = 0;
                                rdmsr(MSR_HYPERV_SIEFP, &eax, &edx);
                                siefp.raw = ((UQUAD)edx << 32) | eax;
                                siefp.u.siefp_enabled = 1;
                                siefp.u.base_siefp_gpa = ((IPTR)data->vbhv_SynICEventPage >> PAGE_SHIFT);
                                wrmsr(MSR_HYPERV_SIEFP, siefp.raw);
                                eax = edx = 0;
                                rdmsr(MSR_HYPERV_SIEFP, &eax, &edx);
                                siefp.raw = ((UQUAD)edx << 32) | eax;

                                if (siefp.u.siefp_enabled)
                                {
                                    D(bug("[VMBusHV:VMBus] %s: Configuring Syn Int (vector %u) ..\n", __func__, apicBusIRQ);)
                                    shared_sint.u.vector = apicBusIRQ;
                                    shared_sint.u.masked = FALSE;
                                    shared_sint.u.auto_eoi = FALSE;
                                    wrmsr(MSR_HYPERV_SINT0 + HV_VMBUS_MESSAGE_SINT,
                                        shared_sint.raw);

                                    D(bug("[VMBusHV:VMBus] %s: Enabling SynIC ..\n", __func__);)
                                    /* Enable the global synic bit */
                                    eax = edx = 0;
                                    rdmsr(MSR_HYPERV_SCONTROL, &eax, &edx);
                                    sctrl.raw = ((UQUAD)edx << 32) | eax;
                                    sctrl.u.enable = 1;
                                    wrmsr(MSR_HYPERV_SCONTROL, sctrl.raw);
                                    eax = edx = 0;
                                    rdmsr(MSR_HYPERV_SCONTROL, &eax, &edx);
                                    sctrl.raw = ((UQUAD)edx << 32) | eax;
                                    if (sctrl.u.enable)
                                    {
                                        D(bug("[VMBusHV:VMBus] %s: SynIC is configured and running\n", __func__);)
                                        vmbusok = TRUE;
                                    }
                                    else
                                    {
                                        D(bug("[VMBusHV:VMBus] %s: Failed to enable SynIC\n", __func__);)
                                    }
                                }
                                else
                                {
                                    D(bug("[VMBusHV:VMBus] %s: Failed to enable SynIC Event Page!\n", __func__);)
                                }
                            }
                            else
                            {
                                D(bug("[VMBusHV:VMBus] %s: Failed to enable SynIC Msg Page!\n", __func__);)
                            }

                            UserState(ssp);
                        }
 
                        if (vmbusok)
                        {
                            D(bug("[VMBusHV:VMBus] %s: Calling Channel Init ...\n", __func__);)
                            if (!VMBusHV_DoChannelInitMsg(data))
                            {
                                D(bug("[VMBusHV:VMBus] %s: Calling Channel Request-Offers ...\n", __func__);)
                                VMBusHV_DoChannelOffersMsg(data);
                            }
                        }
                    }
                }

                if (!vmbusok)
                {
                    D(bug("[VMBusHV:VMBus] %s: Disposing Bus Object...\n", __func__);)
                    OOP_MethodID disp_msg = OOP_GetMethodID(IID_Root, moRoot_Dispose);
                    OOP_DoSuperMethod(PSD(cl)->vmbushvBusClass, busObj, &disp_msg);
                    busObj = NULL;
                }
            }
        }
        PSD(cl)->hvbusObj = busObj;
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

    D(bug("[VMBusHV:VMBus] %s()\n", __func__);)

    if (!handled)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    }
}

void VMBusHV__Hidd_VMBus__EnumUnits(OOP_Class *cl, OOP_Object *o, struct pHidd_VMBus_EnumUnits *msg)
{
    struct VMBusHVBusData *data = OOP_INST_DATA(cl, o);
    struct vmbushv_staticdata *psd = PSD(cl);

    D(bug("[VMBusHV:VMBus] %s()\n", __func__);)
}


#undef _psd
#undef UtilityBase
#define UtilityBase (psd->utilityBase)

#undef OOPBase
#undef HiddPCIVMBusAttrBase
#define HiddPCIVMBusAttrBase    (_psd->hiddPCIVMBusAB)
#undef HWBase
#define HWBase                  (_psd->hwMethodBase)

static BOOL VMBusHV_isMSHyperV(void)
{
        ULONG arg = 1;
        ULONG res[4];

        asm volatile (
            "cpuid"
                : "=c"(res[0])
                : "a"(arg)
                : "%ebx", "%edx"
        );

        if (res[0] & 0x80000000) {
            /* Hypervisor Flag .. */
            arg = 0x40000000;
            asm volatile (
                "cpuid"
                    : "=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3])
                    : "a"(arg)
                    : 
            );
            /*
             * res[0]           contains the number of CPUID functions
             * res[1 - 3]       contain the hypervisor signature...
            */
            if ((res[0] >= HYPERV_CPUID_MIN && res[0] <= HYPERV_CPUID_MAX) &&
                (res[1] == 0x7263694d) &&       /* 'r','c','i','M' */
                (res[2] == 0x666f736f) &&       /* 'f','o','s','o' */
                (res[3] == 0x76482074))         /* 'v','H',' ','t' */
            {
                    return TRUE;
            }
        }
	return FALSE;
}

static ULONG VMBusHV_MSHyperVVers(void)
{
    ULONG arg = 1, vers;
    ULONG res[4];

    arg = HYPERV_CPUID_MAXANDVENDOR;
    asm volatile (
        "cpuid"
            : "=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3])
            : "a"(arg)
            : 
    );
    vers = res[0];

    arg = HYPERV_CPUID_INTERFACE;
    asm volatile (
        "cpuid"
            : "=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3])
            : "a"(arg)
            : 
    );

    if (vers >= HYPERV_CPUID_VERSION) {
        arg = HYPERV_CPUID_VERSION;
        asm volatile (
            "cpuid"
                : "=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3])
                : "a"(arg)
                : 
        );
    }
    return (vers);
}

static int VMBusHV_InitClass(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;
    struct vmbushv_staticdata *_psd = &LIBBASE->psd;
    CONST_STRPTR const attrBaseIDs[] =
    {
        IID_Hidd,
        IID_HW,
        NULL
    };

    D(bug("[VMBusHV:VMBus] %s()\n", __func__));

    if (!OOP_ObtainAttrBasesArray(&LIBBASE->psd.hiddAB, attrBaseIDs))
    {
        if (VMBusHV_isMSHyperV())
        {
            ULONG maxcpuid = VMBusHV_MSHyperVVers();

            D(bug("[VMBusHV:VMBus] %s: HyperV detected (cpuid max = %u), setting up Bus..\n", __func__, maxcpuid));

            OOP_Object *root = OOP_NewObject(NULL, CLID_Hidd_System, NULL);
            if (!root)
                root = OOP_NewObject(NULL, CLID_HW_Root, NULL);

            if (HW_AddDriver(root, LIBBASE->psd.vmbushvBusClass, NULL))
            {
                D(bug("[VMBusHV:VMBus] %s: Init complete\n", __func__));

                return TRUE;
            }
        }
        else
        {
            D(bug("[VMBusHV:VMBus] %s: not running on a hyper-v guest\n", __func__));    
        }
    }
    else
    {
        D(bug("[VMBusHV:VMBus] %s: failed to obtain Atrr bases\n", __func__));    
    }
    D(bug("[VMBusHV:VMBus] %s: Init failed\n", __func__));

    return FALSE;
}

static int VMBusHV_ExpungeClass(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase = LIBBASE->psd.OOPBase;

    D(bug("[VMBusHV:VMBus] %s()\n", __func__));

    OOP_ReleaseAttrBase(IID_Hidd);

    return TRUE;
}

ADD2INITLIB(VMBusHV_InitClass, 10)
ADD2EXPUNGELIB(VMBusHV_ExpungeClass, 0)

