/*
    Copyright © 2020, The AROS Development Team. All rights reserved.
    $Id$
    Lang: English
*/


#include <aros/debug.h>

#include <aros/symbolsets.h>
#include "vmbus-hv_intern.h"

/* Perform the requested hypercall */
static UQUAD VMBusHV_DoHypercall(struct VMBusHVBusData *data, UQUAD control, void* send, void* rcv)
{
    volatile void* hypercall_page = data->vbhv_HCPage;
    UQUAD hcstatus = 0;
    APTR ssp;

    D(bug("[VMBusHV:VMBus] %s(0x%p)\n", __func__, data);)

    if ((ssp = SuperState()) != NULL)
    {
#if defined(__x86_64__)
        asm volatile ("mov %0, %%r8" : : "r" (rcv): "r8");
        asm volatile ("call *%3" : "=a"(hcstatus) : "c" (control), "d" (send), "m" (hypercall_page));
#else
        ULONG send_address_lo = ((IPTR)send & 0xFFFFFFFF);
        ULONG rcv_address_lo = ((IPTR)rcv & 0xFFFFFFFF);

        ULONG control_hi = control >> 32;
        ULONG control_lo = control & 0xFFFFFFFF;

        ULONG hcstatus_hi = 1, hcstatus_lo = 1;

        asm volatile ("call *%8" : "=d"(hcstatus_hi), "=a"(hcstatus_lo) : "d" (control_hi), "a" (control_lo), "b" (0), "c" (send_address_lo), "D"(0), "S"(rcv_address_lo), "m" (hypercall_page));

        hcstatus = (hcstatus_lo | ((UQUAD)hcstatus_hi << 32));
#endif /* __x86_64__ */
        UserState(ssp);
    }
    D(bug("[VMBusHV:VMBus] %s: returning %08x%08x\n", __func__, (hcstatus >> 32) & 0xFFFFFFFF, hcstatus & 0xFFFFFFFF);)

    return (hcstatus);
}

/* Perform the channel init request */
ULONG VMBusHV_DoChannelInitMsg(struct VMBusHVBusData *data)
{
    struct inputmsg {
        UQUAD alignment8;
        hv_vmbus_input_post_message msg;
    };
    hv_vmbus_input_post_message *hcMsg;
    hv_vmbus_channel_initiate_contact *icMsg;
    ULONG status;
    APTR msgAlloc;

    D(bug("[VMBusHV:VMBus] %s(0x%p)\n", __func__, data);)

    msgAlloc = AllocMem(sizeof(struct inputmsg) + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
    hcMsg = (APTR)(((IPTR)msgAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));
    icMsg = (APTR)hcMsg->payload;

    D(bug("[VMBusHV:VMBus] %s: Hypercall Msg @ 0x%p, InitContat Msg @ 0x%p\n", __func__, hcMsg, icMsg);)

    /* interrupt page */
    data->vbhv_IPAlloc = AllocMem(PAGE_SIZE + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
    data->vbhv_IPPage = (APTR)(((IPTR)data->vbhv_IPAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));

    D(bug("[VMBusHV:VMBus] %s: Interrupt Buffer @ 0x%p (alloc @ 0x%p)\n", __func__, data->vbhv_IPPage, data->vbhv_IPAlloc);)

    /* monitor pages */
    data->vbhv_MPAlloc = AllocMem(PAGE_SIZE + PAGE_SIZE + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
    data->vbhv_MPPage = (APTR)(((IPTR)data->vbhv_MPAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));

    D(bug("[VMBusHV:VMBus] %s: Monitor Buffer @ 0x%p (alloc @ 0x%p)\n", __func__, data->vbhv_MPPage, data->vbhv_MPAlloc);)

    icMsg->header.message_type = HV_CHANNEL_MESSAGE_INITIATED_CONTACT;
    icMsg->vmbus_version_requested = HV_VMBUS_REVISION_NUMBER;
    icMsg->interrupt_page = (UQUAD)(IPTR)data->vbhv_IPPage;
    icMsg->monitor_page_1 = (UQUAD)(IPTR)data->vbhv_MPPage;
    icMsg->monitor_page_2 = icMsg->monitor_page_1 + PAGE_SIZE;
    D(bug("[VMBusHV:VMBus] %s: Monitor Buffer Page 2 @ 0x%p\n", __func__, icMsg->monitor_page_2);)

    hcMsg->connection_id.raw = 0;
    hcMsg->connection_id.u.id = HV_VMBUS_MESSAGE_CONNECTION_ID;
    hcMsg->message_type = 1;
    hcMsg->payload_size = sizeof(hv_vmbus_channel_initiate_contact);

    status = (ULONG)(VMBusHV_DoHypercall(data, HV_CALL_POST_MESSAGE, hcMsg, 0) & 0xFFFFFFFF);
    D(bug("[VMBusHV:VMBus] %s: DoHypercall returned, status %04x\n", __func__, ((UWORD)status & 0xFFFF));)

    FreeMem(msgAlloc, sizeof(struct inputmsg) + (PAGE_SIZE - 1));

    return (status);
}

/* Send a request to get pending offers */
ULONG VMBusHV_DoChannelOffersMsg(struct VMBusHVBusData *data)
{
    struct inputmsg {
        UQUAD alignment8;
        hv_vmbus_input_post_message msg;
    };
    hv_vmbus_input_post_message *hcMsg;
    hv_vmbus_channel_msg_header *coMsg;
    ULONG status;
    APTR msgAlloc;

    D(bug("[VMBusHV:VMBus] %s(0x%p)\n", __func__, data);)

    msgAlloc = AllocMem(sizeof(struct inputmsg) + (PAGE_SIZE - 1), MEMF_31BIT|MEMF_CLEAR);
    hcMsg = (APTR)(((IPTR)msgAlloc + PAGE_SIZE) & ~(PAGE_SIZE - 1));
    coMsg = (APTR)hcMsg->payload;

    coMsg->message_type = HV_CHANNEL_MESSAGE_REQUEST_OFFERS;
    hcMsg->connection_id.raw = 0;
    hcMsg->connection_id.u.id = HV_VMBUS_MESSAGE_CONNECTION_ID;
    hcMsg->message_type = 1;
    hcMsg->payload_size = sizeof(hv_vmbus_channel_msg_header);

    status = (ULONG)(VMBusHV_DoHypercall(data, HV_CALL_POST_MESSAGE, hcMsg, 0) & 0xFFFFFFFF);
    D(bug("[VMBusHV:VMBus] %s: DoHypercall returned, status %04x\n", __func__, ((UWORD)status & 0xFFFF));)

    FreeMem(msgAlloc, sizeof(struct inputmsg) + (PAGE_SIZE - 1));

    return (status);
}

