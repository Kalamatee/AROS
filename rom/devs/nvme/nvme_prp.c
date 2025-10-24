/*
    Copyright (C) 2023-2025, The AROS Development Team. All rights reserved
*/

#include <proto/exec.h>

/* We want all other bases obtained from our base */
#define __NOLIBBASE__

#include <proto/kernel.h>
#include <proto/timer.h>
#include <proto/bootloader.h>
#include <proto/expansion.h>
#include <proto/utility.h>

#include <aros/atomic.h>
#include <aros/symbolsets.h>
#include <exec/exec.h>
#include <exec/resident.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <utility/utility.h>
#include <libraries/expansion.h>
#include <libraries/configvars.h>
#include <devices/trackdisk.h>
#include <devices/newstyle.h>
#include <dos/bptr.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include <hidd/pci.h>
#include <interface/Hidd_PCIDriver.h>

#include <string.h>

#include "nvme_debug.h"
#include "nvme_intern.h"
#include "nvme_queue_io.h"

#include LC_LIBDEFS_FILE

#ifndef DMA_Continue
#define DMA_Continue    (1L << 1)
#endif

#define NVME_CMD_PSDT_MASK      (3 << 6)

BOOL nvme_initprp(struct nvme_command *cmdio, struct completionevent_handler *ioehandle, struct nvme_Unit *unit, ULONG len, APTR *data, BOOL is_write)
{
    device_t dev = unit->au_Bus->ab_Dev;
    ULONG page_size;
    ULONG page_mask;
    ULONG first_seg_len = len;
    ULONG dma_flags = is_write ? DMAFLAGS_PREWRITE : DMAFLAGS_PREREAD;
    APTR phys1;
    UQUAD dma_addr1;
    ULONG first_chunk;

    ioehandle->ceh_IOMem.me_Un.meu_Addr = NULL;

    if (!dev || !dev->dev_PCIDriverObject) {
        return FALSE;
    }

    page_size = dev->pagesize;
    page_mask = page_size - 1;

    phys1 = CachePreDMA(*data, &first_seg_len, dma_flags);
    if (!phys1) {
        return FALSE;
    }

    dma_addr1 = (UQUAD)(IPTR)HIDD_PCIDriver_CPUtoPCI(dev->dev_PCIDriverObject, phys1);
    first_chunk = MIN(len, page_size - (dma_addr1 & page_mask));

    cmdio->rw.op.flags &= ~NVME_CMD_PSDT_MASK;
    cmdio->rw.prp1 = AROS_QUAD2LE(dma_addr1);

    if (len <= first_chunk) {
        cmdio->rw.prp2 = 0;
        return TRUE;
    }

    {
        ULONG remaining = len - first_chunk;
        ULONG second_seg_len = remaining;
        APTR phys2 = CachePreDMA((APTR)((UBYTE *)(*data) + first_chunk), &second_seg_len, dma_flags | DMA_Continue);

        if (!phys2) {
            return FALSE;
        }

        UQUAD dma_addr2 = (UQUAD)(IPTR)HIDD_PCIDriver_CPUtoPCI(dev->dev_PCIDriverObject, phys2);

        if ((dma_addr2 & page_mask) != 0) {
            return FALSE;
        }

        if ((remaining > page_size) || (second_seg_len < remaining)) {
            return FALSE;
        }

        cmdio->rw.prp2 = AROS_QUAD2LE(dma_addr2);
    }

    return TRUE;
}
