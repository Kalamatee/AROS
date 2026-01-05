/*
    Copyright (C) 2010-2026, The AROS Development Team. All rights reserved

    Desc: EHCI chipset driver root hub/port support functions
*/

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/oop.h>

#include <hidd/pci.h>
#include <utility/hooks.h>
#include <exec/memory.h>

#include <devices/usb_hub.h>

#include "uhwcmd.h"
#include "ehciproto.h"
#include "ohci/ohcichip.h"
#include "uhci/uhcichip.h"

#ifdef base
#undef base
#endif
#define base (hc->hc_Device)
#if defined(AROS_USE_LOGRES)
#ifdef LogHandle
#undef LogHandle
#endif
#ifdef LogResBase
#undef LogResBase
#endif
#define LogHandle (hc->hc_LogRHandle)
#define LogResBase (base->hd_LogResBase)
#endif

void ehciCheckPortStatusChange(struct PCIController *hc)
{
    struct PCIUnit *unit = hc->hc_Unit;
    UWORD hciport;
    ULONG oldval;
    UWORD portreg = EHCI_PORTSC1;

    for(hciport = 0; hciport < hc->hc_NumPorts; hciport++, portreg += 4) {
        oldval = READREG32_LE(hc->hc_RegBase, portreg);
        // reflect port ownership (shortcut without hc->hc_PortNum[hciport], as usb 2.0 maps 1:1)
        unit->hu_PortOwner[hciport] = (oldval & EHPF_NOTPORTOWNER) ? HCITYPE_UHCI : HCITYPE_EHCI;
        if(oldval & EHPF_ENABLECHANGE) {
            hc->hc_PortChangeMap[hciport] |= UPSF_PORT_ENABLE;
        }
        if(oldval & EHPF_CONNECTCHANGE) {
            hc->hc_PortChangeMap[hciport] |= UPSF_PORT_CONNECTION;
        }
        if(oldval & EHPF_RESUMEDTX) {
            hc->hc_PortChangeMap[hciport] |= UPSF_PORT_SUSPEND|UPSF_PORT_ENABLE;
        }
        if(oldval & EHPF_OVERCURRENTCHG) {
            hc->hc_PortChangeMap[hciport] |= UPSF_PORT_OVER_CURRENT;
        }
        WRITEREG32_LE(hc->hc_RegBase, portreg, oldval);
        pciusbEHCIDebug("EHCI", "PCI Int Port %ld Change %08lx\n", hciport + 1, oldval);
        if(hc->hc_PortChangeMap[hciport]) {
            unit->hu_RootPortChanges |= 1UL<<(hciport + 1);
        }
    }

    uhwCheckRootHubChanges(unit);
}

BOOL ehciSetFeature(struct PCIUnit *unit, struct PCIController *hc, UWORD hciport, UWORD idx, UWORD val, WORD *retval)
{
    UWORD portreg = EHCI_PORTSC1 + (hciport<<2);
    ULONG oldval = READREG32_LE(hc->hc_RegBase, portreg) & ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE); // these are clear-on-write!
    ULONG newval = oldval;
    ULONG cnt;
    BOOL cmdgood = FALSE;

    pciusbEHCIDebug("EHCI", "%s(0x%p, 0x%p, %04x, %04x, %04x, 0x%p)\n", __func__, unit, hc, hciport, idx, val, retval);

    switch(val) {
    /* case UFS_PORT_CONNECTION: not possible */
    case UFS_PORT_ENABLE:
        pciusbEHCIDebug("EHCI", "Enabling Port (%s)\n", newval & EHPF_PORTENABLE ? "already" : "ok");
        newval |= EHPF_PORTENABLE;
        cmdgood = TRUE;
        break;

    case UFS_PORT_SUSPEND:
        newval |= EHPF_PORTSUSPEND;
        hc->hc_PortChangeMap[hciport] |= UPSF_PORT_SUSPEND; // manually fake suspend change
        cmdgood = TRUE;
        break;

    case UFS_PORT_RESET:
        pciusbEHCIDebug("EHCI", "Resetting Port (%s)\n", newval & EHPF_PORTRESET ? "already" : "ok");

        if(hc->hc_CompanionHC) {
            ULONG resetval = 0;
            UWORD portind = 1;
            struct PCIController *chc = hc->hc_CompanionHC;
            ULONG compport = unit->hu_PortNum11[idx - 1];
            ULONG compmap = hc->hc_portroute ? ((hc->hc_portroute >> ((compport - 1) << 2)) & 0xf) : 0;

            if(compport > 0) {
                /*
                 * for each port, we check the port mapping (compmap). This is a zero-based index.
                 * if the port mapping is not for this controller then we must check the next one.
                 * if it is for this controller then we must reset that port on that controller.
                 */
                while ((chc) && (compmap != 0)) {
                    chc = chc->hc_CompanionHC;
                    compmap--;
                    portind++;
                }
            }

            if(chc) {
                // send a reset to the companion port, then transfer ownership.
                if(chc->hc_HCType == HCITYPE_UHCI) {
                    UWORD uhcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD uhciportreg = uhcihciport ? UHCI_PORT2STSCTRL : UHCI_PORT1STSCTRL;
                    ULONG __unused uhcinewval = READREG16_LE(chc->hc_RegBase, uhciportreg);
                    pciusbEHCIDebug("EHCI", "UHCI Port status before handover=%04lx\n", uhcinewval);
                } else if(chc->hc_HCType == HCITYPE_OHCI) {
                    UWORD ohcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD ohciportreg = OHCI_PORTSTATUS + (ohcihciport<<2);
                    ULONG __unused ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);
                    pciusbEHCIDebug("EHCI", "OHCI Port status before handover=%04lx\n", ohcioldval);
                }
            }

            newval &= ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE|EHPF_PORTSUSPEND|EHPF_PORTENABLE);
            WRITEREG32_LE(hc->hc_RegBase, portreg, newval);

            if(chc) {
                // transfer ownership to UHCI/OHCI.
                if(chc->hc_HCType == HCITYPE_UHCI) {
                    pciusbEHCIDebug("EHCI", "Transferring ownership to UHCI port %ld\n", unit->hu_PortNum11[idx - 1]);
                } else if(chc->hc_HCType == HCITYPE_OHCI) {
                    pciusbEHCIDebug("EHCI", "Transferring ownership to OHCI port %ld\n", unit->hu_PortNum11[idx - 1]);
                }
            }

            if (chc) {
                if(chc->hc_HCType == HCITYPE_UHCI) {
                    UWORD uhcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD uhciportreg = uhcihciport ? UHCI_PORT2STSCTRL : UHCI_PORT1STSCTRL;
                    ULONG uhcinewval;

                    uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND);
                    pciusbEHCIDebug("EHCI", "UHCI Reset=%s\n", uhcinewval & UHPF_PORTRESET ? "BAD!" : "GOOD");
                    if((uhcinewval & UHPF_PORTRESET)) { //|| (newval & EHPF_LINESTATUS_DM))
                        uhcinewval &= ~(UHPF_PORTSUSPEND|UHPF_PORTENABLE);
                        uhcinewval |= UHPF_PORTRESET;
                        WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                        uhwDelayMS(25, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND|UHPF_PORTENABLE);
                        pciusbEHCIDebug("EHCI", "UHCI Re-Reset=%s\n", uhcinewval & UHPF_PORTRESET ? "GOOD" : "BAD!");
                        uhcinewval &= ~UHPF_PORTRESET;
                        WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                        uhwDelayMicro(50, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND);
                        pciusbEHCIDebug("EHCI", "UHCI Re-Reset=%s\n", uhcinewval & UHPF_PORTRESET ? "STILL BAD!" : "GOOD");
                    }
                    uhcinewval &= ~UHPF_PORTRESET;
                    uhcinewval |= UHPF_PORTENABLE;
                    WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                    chc->hc_PortChangeMap[uhcihciport] |= UPSF_PORT_RESET|UPSF_PORT_ENABLE; // manually fake reset change

                    cnt = 100;
                    do {
                        uhwDelayMS(1, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg);
                    } while(--cnt && (!(uhcinewval & UHPF_PORTENABLE)));
                    if(cnt) {
                        pciusbEHCIDebug("EHCI", "Enabled after %ld ticks\n", 100-cnt);
                    } else {
                        pciusbWarn("EHCI", "Port refuses to be enabled!\n");
                        *retval = UHIOERR_HOSTERROR;
                        return TRUE;
                    }
                } else if(chc->hc_HCType == HCITYPE_OHCI) {
                    UWORD ohcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD ohciportreg = OHCI_PORTSTATUS + (ohcihciport<<2);
                    ULONG ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);

                    // make sure we have at least 50ms of reset time here, as required for a root hub port
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);
                    while(ohcioldval & OHPF_PORTRESET) {
                        ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);
                    }
                }
            }

            if(chc) {
                newval = READREG32_LE(hc->hc_RegBase, portreg) & ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE|EHPF_PORTSUSPEND);
                WRITEREG32_LE(hc->hc_RegBase, portreg, newval);
                pciusbEHCIDebug("EHCI", "Port status (after handover)=%08lx\n", READREG32_LE(hc->hc_RegBase, portreg) & ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE|EHPF_PORTSUSPEND));
                // enable companion controller port
                if(chc->hc_HCType == HCITYPE_UHCI) {
                    UWORD uhcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD uhciportreg = uhcihciport ? UHCI_PORT2STSCTRL : UHCI_PORT1STSCTRL;
                    ULONG uhcinewval;

                    uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND);
                    if((uhcinewval & UHPF_PORTRESET)) { //|| (newval & EHPF_LINESTATUS_DM))
                        uhcinewval &= ~(UHPF_PORTSUSPEND|UHPF_PORTENABLE);
                        uhcinewval |= UHPF_PORTRESET;
                        WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                        uhwDelayMS(25, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND|UHPF_PORTENABLE);
                        uhcinewval &= ~UHPF_PORTRESET;
                        WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                        uhwDelayMicro(50, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg) & ~(UHPF_ENABLECHANGE|UHPF_CONNECTCHANGE|UHPF_PORTSUSPEND);
                    }
                    uhcinewval &= ~UHPF_PORTRESET;
                    uhcinewval |= UHPF_PORTENABLE;
                    WRITEIO16_LE(chc->hc_RegBase, uhciportreg, uhcinewval);
                    chc->hc_PortChangeMap[uhcihciport] |= UPSF_PORT_RESET|UPSF_PORT_ENABLE; // manually fake reset change

                    cnt = 100;
                    do {
                        uhwDelayMS(1, unit->hu_TimerReq);
                        uhcinewval = READIO16_LE(chc->hc_RegBase, uhciportreg);
                    } while(--cnt && (!(uhcinewval & UHPF_PORTENABLE)));
                    if(cnt) {
                        pciusbEHCIDebug("EHCI", "Enabled after %ld ticks\n", 100-cnt);
                    } else {
                        pciusbWarn("EHCI", "Port refuses to be enabled!\n");
                        *retval = UHIOERR_HOSTERROR;
                        return TRUE;
                    }
                } else if(chc->hc_HCType == HCITYPE_OHCI) {
                    UWORD ohcihciport = unit->hu_PortNum11[idx - 1];
                    UWORD ohciportreg = OHCI_PORTSTATUS + (ohcihciport<<2);
                    ULONG ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);

                    // make sure we have at least 50ms of reset time here, as required for a root hub port
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    WRITEREG32_LE(chc->hc_RegBase, ohciportreg, OHPF_PORTRESET);
                    uhwDelayMS(10, unit->hu_TimerReq);
                    ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);
                    while(ohcioldval & OHPF_PORTRESET) {
                        ohcioldval = READREG32_LE(chc->hc_RegBase, ohciportreg);
                    }
                }
            }

            newval = READREG32_LE(hc->hc_RegBase, portreg) & ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE|EHPF_PORTSUSPEND);
            WRITEREG32_LE(hc->hc_RegBase, portreg, newval);
            hc->hc_PortChangeMap[hciport] |= UPSF_PORT_RESET; // manually fake reset change

            cnt = 100;
            do {
                uhwDelayMS(1, unit->hu_TimerReq);
                newval = READREG32_LE(hc->hc_RegBase, portreg);
            } while(--cnt && (!(newval & EHPF_PORTENABLE)));
            if(cnt) {
                pciusbEHCIDebug("EHCI", "Enabled after %ld ticks\n", 100-cnt);
            } else {
                pciusbWarn("EHCI", "Port refuses to be enabled!\n");
                *retval = UHIOERR_HOSTERROR;
                return TRUE;
            }

            // make enumeration possible
            unit->hu_DevControllers[0] = hc;
            cmdgood = TRUE;
        }
        break;

    case UFS_PORT_POWER:
        pciusbEHCIDebug("EHCI", "Powering Port\n");
        newval |= EHPF_PORTPOWER;
        cmdgood = TRUE;
        break;

        /* case UFS_PORT_LOW_SPEED: not possible */
        /* case UFS_C_PORT_CONNECTION:
        case UFS_C_PORT_ENABLE:
        case UFS_C_PORT_SUSPEND:
        case UFS_C_PORT_OVER_CURRENT:
        case UFS_C_PORT_RESET: */
    }
    if(cmdgood) {
        pciusbEHCIDebug("EHCI", "Port %ld SET_FEATURE %04lx->%04lx\n", idx, oldval, newval);
        WRITEREG32_LE(hc->hc_RegBase, portreg, newval);
    }
    return cmdgood;
}

BOOL ehciClearFeature(struct PCIUnit *unit, struct PCIController *hc, UWORD hciport, UWORD idx, UWORD val, WORD *retval)
{
    UWORD portreg = EHCI_PORTSC1 + (hciport<<2);
    ULONG oldval = READREG32_LE(hc->hc_RegBase, portreg) & ~(EHPF_OVERCURRENTCHG|EHPF_ENABLECHANGE|EHPF_CONNECTCHANGE); // these are clear-on-write!
    ULONG newval = oldval;
    BOOL cmdgood = FALSE;

    pciusbEHCIDebug("EHCI", "%s(0x%p, 0x%p, %04x, %04x, %04x, 0x%p)\n", __func__, unit, hc, hciport, idx, val, retval);

    switch(val) {
    case UFS_PORT_ENABLE:
        pciusbEHCIDebug("EHCI", "Disabling Port (%s)\n", newval & EHPF_PORTENABLE ? "ok" : "already");
        newval &= ~EHPF_PORTENABLE;
        cmdgood = TRUE;
        // disable enumeration
        unit->hu_DevControllers[0] = NULL;
        break;

    case UFS_PORT_SUSPEND:
        newval &= ~EHPF_PORTSUSPEND;
        cmdgood = TRUE;
        break;

    case UFS_PORT_POWER:
        pciusbEHCIDebug("EHCI", "Disabling Power\n");
        newval &= ~EHPF_PORTPOWER;
        cmdgood = TRUE;
        break;

    case UFS_C_PORT_CONNECTION:
        newval |= EHPF_CONNECTCHANGE; // clear-on-write!
        hc->hc_PortChangeMap[hciport] &= ~UPSF_PORT_CONNECTION;
        cmdgood = TRUE;
        break;

    case UFS_C_PORT_ENABLE:
        newval |= EHPF_ENABLECHANGE; // clear-on-write!
        hc->hc_PortChangeMap[hciport] &= ~UPSF_PORT_ENABLE;
        cmdgood = TRUE;
        break;

    case UFS_C_PORT_SUSPEND:
        newval |= EHPF_RESUMEDTX; // clear-on-write!
        hc->hc_PortChangeMap[hciport] &= ~UPSF_PORT_SUSPEND; // manually fake suspend change clearing
        cmdgood = TRUE;
        break;

    case UFS_C_PORT_OVER_CURRENT:
        newval |= EHPF_OVERCURRENTCHG; // clear-on-write!
        hc->hc_PortChangeMap[hciport] &= ~UPSF_PORT_OVER_CURRENT; // manually fake over current clearing
        cmdgood = TRUE;
        break;

    case UFS_C_PORT_RESET:
        hc->hc_PortChangeMap[hciport] &= ~UPSF_PORT_RESET; // manually fake reset change clearing
        cmdgood = TRUE;
        break;
    }
    if(cmdgood) {
        pciusbEHCIDebug("EHCI", "Port %ld CLEAR_FEATURE %04lx->%04lx\n", idx, oldval, newval);
        WRITEREG32_LE(hc->hc_RegBase, portreg, newval);
        if(hc->hc_PortChangeMap[hciport]) {
            unit->hu_RootPortChanges |= 1UL<<idx;
        } else {
            unit->hu_RootPortChanges &= ~(1UL<<idx);
        }
    }
    return cmdgood;
}

BOOL ehciGetStatus(struct PCIController *hc, UWORD *mptr, UWORD hciport, UWORD idx, WORD *retval)
{
    UWORD portreg = EHCI_PORTSC1 + (hciport<<2);
    ULONG oldval = READREG32_LE(hc->hc_RegBase, portreg);

    pciusbEHCIDebug("EHCI", "%s(0x%p, 0x%p, %04x, %04x, 0x%p)\n", __func__, hc, mptr, hciport, idx, retval);

    *mptr = AROS_WORD2LE(UPSF_PORT_POWER);

    if(oldval & EHPF_PORTCONNECTED) *mptr |= AROS_WORD2LE(UPSF_PORT_CONNECTION);
    if(oldval & EHPF_PORTENABLE) *mptr |= AROS_WORD2LE(UPSF_PORT_ENABLE);
    if(oldval & EHPF_PORTSUSPEND) *mptr |= AROS_WORD2LE(UPSF_PORT_SUSPEND);
    if((oldval & EHPF_LINESTATUS) == EHPF_LINESTATUS_KSTATE) *mptr |= AROS_WORD2LE(UPSF_PORT_LOW_SPEED);
    if((oldval & EHPF_LINESTATUS) == EHPF_LINESTATUS_JSTATE) *mptr |= AROS_WORD2LE(UPSF_PORT_LOW_SPEED);
    if(oldval & EHPF_PORTRESET) *mptr |= AROS_WORD2LE(UPSF_PORT_RESET);
    if(oldval & EHPF_OVERCURRENT) *mptr |= AROS_WORD2LE(UPSF_PORT_OVER_CURRENT);

    pciusbEHCIDebug("EHCI", "Port %ld Status %08lx\n", idx, *mptr);

    mptr++;
    if(oldval & EHPF_ENABLECHANGE) {
        hc->hc_PortChangeMap[hciport] |= UPSF_PORT_ENABLE;
    }
    if(oldval & EHPF_CONNECTCHANGE) {
        hc->hc_PortChangeMap[hciport] |= UPSF_PORT_CONNECTION;
    }
    if(oldval & EHPF_RESUMEDTX) {
        hc->hc_PortChangeMap[hciport] |= UPSF_PORT_SUSPEND|UPSF_PORT_ENABLE;
    }
    if(oldval & EHPF_OVERCURRENTCHG) {
        hc->hc_PortChangeMap[hciport] |= UPSF_PORT_OVER_CURRENT;
    }

    *mptr = AROS_WORD2LE(hc->hc_PortChangeMap[hciport]);
    WRITEREG32_LE(hc->hc_RegBase, portreg, oldval);

    pciusbEHCIDebug("EHCI", "Port %ld Change %08lx\n", idx, *mptr);

    return TRUE;
}

#if defined(AROS_USE_LOGRES)
#undef LogResBase
#undef LogHandle
#endif
#undef base
