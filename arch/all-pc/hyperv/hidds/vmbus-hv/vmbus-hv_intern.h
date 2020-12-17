/*
    Copyright © 2020, The AROS Development Team. All rights reserved.
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

#define IID_Hidd_VMBus "hidd.bus.vmbus"

static inline void __attribute__((always_inline)) rdmsr(ULONG msr_no, ULONG *ret_lo, ULONG *ret_hi)
{
    ULONG lowval,highval;
    asm volatile("rdmsr":"=a"(lowval),"=d"(highval):"c"(msr_no));
    *ret_lo=lowval;
    *ret_hi=highval;
}

static inline void __attribute__((always_inline)) wrmsr(ULONG msr_no, UQUAD msr_value)
{
    ULONG lowval = (msr_value & 0xFFFFFFFF), highval = ((msr_value >> 32) & 0xFFFFFFFF);
    asm volatile ( "wrmsr" : : "c" (msr_no), "a" (lowval),"d" (highval) );
}

#define PAGE_SHIFT                              12
#define PAGE_SIZE                               (1UL << PAGE_SHIFT)

#define HYPERV_ID_BASE			        0x40000000

/*
 * MS HyperV CPUID definitions
 */

#define HYPERV_CPUID_MIN			(HYPERV_ID_BASE + 0x00000005)
#define HYPERV_CPUID_MAX			(HYPERV_ID_BASE + 0x0000ffff)
 
#define HYPERV_CPUID_MAXANDVENDOR		(HYPERV_ID_BASE + 0x00000000)
#define HYPERV_CPUID_INTERFACE			(HYPERV_ID_BASE + 0x00000001)
#define HYPERV_CPUID_VERSION			(HYPERV_ID_BASE + 0x00000002)
#define HYPERV_CPUID_FEATURES			(HYPERV_ID_BASE + 0x00000003)
#define HYPERV_CPUID_ENLIGHT_INFO		(HYPERV_ID_BASE + 0x00000004)
#define HYPERV_CPUID_IMPLM_LIMIT		(HYPERV_ID_BASE + 0x00000005)

/*
 * MS HyperV MSR definitions
 */
#define MSR_HYPERV_GUESTOS_ID	                (HYPERV_ID_BASE + 0x00000000)              /* Set the GuestOS ID                                   */
#define MSR_HYPERV_HYPERCALL	                (HYPERV_ID_BASE + 0x00000001)              /* Setup pages used to communicate with the hypervisor  */
#define MSR_HYPERV_TSC_FREQUENCY		(HYPERV_ID_BASE + 0x00000022)		/* TSC frequency                                        */
#define MSR_HYPERV_APIC_FREQUENCY		(HYPERV_ID_BASE + 0x00000023)		/* APIC timer frequency                                 */

/* synthetic interrupt controller model specific registers */
#define MSR_HYPERV_SCONTROL                     (HYPERV_ID_BASE + 0x00000080)
#define MSR_HYPERV_SVERSION                     (HYPERV_ID_BASE + 0x00000081)
#define MSR_HYPERV_SIEFP                        (HYPERV_ID_BASE + 0x00000082)
#define MSR_HYPERV_SIMP                         (HYPERV_ID_BASE + 0x00000083)
#define MSR_HYPERV_EOM                          (HYPERV_ID_BASE + 0x00000084)

#define MSR_HYPERV_SINT0                        (HYPERV_ID_BASE + 0x00000090)
#define MSR_HYPERV_SINT1                        (HYPERV_ID_BASE + 0x00000091)
#define MSR_HYPERV_SINT2                        (HYPERV_ID_BASE + 0x00000092)
#define MSR_HYPERV_SINT3                        (HYPERV_ID_BASE + 0x00000093)
#define MSR_HYPERV_SINT4                        (HYPERV_ID_BASE + 0x00000094)
#define MSR_HYPERV_SINT5                        (HYPERV_ID_BASE + 0x00000095)
#define MSR_HYPERV_SINT6                        (HYPERV_ID_BASE + 0x00000096)
#define MSR_HYPERV_SINT7                        (HYPERV_ID_BASE + 0x00000097)
#define MSR_HYPERV_SINT8                        (HYPERV_ID_BASE + 0x00000098)
#define MSR_HYPERV_SINT9                        (HYPERV_ID_BASE + 0x00000099)
#define MSR_HYPERV_SINT10                       (HYPERV_ID_BASE + 0x0000009A)
#define MSR_HYPERV_SINT11                       (HYPERV_ID_BASE + 0x0000009B)
#define MSR_HYPERV_SINT12                       (HYPERV_ID_BASE + 0x0000009C)
#define MSR_HYPERV_SINT13                       (HYPERV_ID_BASE + 0x0000009D)
#define MSR_HYPERV_SINT14                       (HYPERV_ID_BASE + 0x0000009E)
#define MSR_HYPERV_SINT15                       (HYPERV_ID_BASE + 0x0000009F)
/*
 * A revision number of vmbus that is used for ensuring both ends on a
 * partition are using compatible versions.
 */

#define HV_VMBUS_REVISION_NUMBER	        13



/*
 * Define the format of the SIMP register
 */
typedef union {
	struct {
		UQUAD simp_enabled	: 1;
		UQUAD preserved	: 11;
		UQUAD base_simp_gpa	: 52;
	} u;
	UQUAD raw;
} hv_vmbus_synic_simp;

/*
 * Define the format of the SIEFP register
 */
typedef union {
	struct {
		UQUAD siefp_enabled	: 1;
		UQUAD preserved	: 11;
		UQUAD base_siefp_gpa	: 52;
	} u;
	UQUAD raw;
} hv_vmbus_synic_siefp;

/*
 * Define synthetic interrupt source
 */
typedef union {
	struct {
		UQUAD vector		: 8;
		UQUAD rsrvd1	: 8;
		UQUAD masked		: 1;
		UQUAD auto_eoi	: 1;
		UQUAD rsrvd2	: 46;
	} u;
	UQUAD raw;
} hv_vmbus_synic_sint;

/*
 * Define syn_ic control register
 */
typedef union _hv_vmbus_synic_scontrol {
    struct {
        UQUAD enable		: 1;
        UQUAD rsrvd	: 63;
    } u;
    UQUAD raw;
} hv_vmbus_synic_scontrol;

/*
 * Define hypervisor message types
 */
typedef enum {
    HV_MESSAGE_TYPE_NONE				= 0x00000000,

    /*
     * Memory access messages
     */
    HV_MESSAGE_TYPE_UNMAPPED_GPA			= 0x80000000,
    HV_MESSAGE_TYPE_GPA_INTERCEPT			= 0x80000001,

    /*
     * Timer notification messages
     */
    HV_MESSAGE_TIMER_EXPIRED			= 0x80000010,

    /*
     * Error messages
     */
    HV_MESSAGE_TYPE_INVALID_VP_REGISTER_VALUE	= 0x80000020,
    HV_MESSAGE_TYPE_UNRECOVERABLE_EXCEPTION		= 0x80000021,
    HV_MESSAGE_TYPE_UNSUPPORTED_FEATURE		= 0x80000022,

    /*
     * Trace buffer complete messages
     */
    HV_MESSAGE_TYPE_EVENT_LOG_BUFFER_COMPLETE	= 0x80000040,

    /*
     * Platform-specific processor intercept messages
     */
    HV_MESSAGE_TYPE_X64_IO_PORT_INTERCEPT		= 0x80010000,
    HV_MESSAGE_TYPE_X64_MSR_INTERCEPT		= 0x80010001,
    HV_MESSAGE_TYPE_X64_CPU_INTERCEPT		= 0x80010002,
    HV_MESSAGE_TYPE_X64_EXCEPTION_INTERCEPT		= 0x80010003,
    HV_MESSAGE_TYPE_X64_APIC_EOI			= 0x80010004,
    HV_MESSAGE_TYPE_X64_LEGACY_FP_ERROR		= 0x80010005

} hv_vmbus_msg_type;

typedef union {
	struct {
		UQUAD enable :1;
		UQUAD rsrvd :11;
		UQUAD guestpa :52;
	} bits;
	UQUAD raw;
} hyperv_vmbus_hypercall_t;

/*
 * Declare the various hypercall operations
 */
typedef enum {
	HV_CALL_POST_MESSAGE	= 0x005c,
	HV_CALL_SIGNAL_EVENT	= 0x005d,
} hv_vmbus_call_code;

enum {
	HV_VMBUS_MESSAGE_CONNECTION_ID	= 1,
	HV_VMBUS_MESSAGE_PORT_ID	= 1,
	HV_VMBUS_EVENT_CONNECTION_ID	= 2,
	HV_VMBUS_EVENT_PORT_ID		= 2,
	HV_VMBUS_MONITOR_CONNECTION_ID	= 3,
	HV_VMBUS_MONITOR_PORT_ID	= 3,
	HV_VMBUS_MESSAGE_SINT		= 2
};

/*
 *  Connection identifier type
 */
typedef union {
	ULONG		        raw;
	struct {
		ULONG	        id:24;
		ULONG	        rsrvd:8;
	} u;

} __attribute__((packed)) hv_vmbus_connection_id;

/*
 *  Define the hv_vmbus_post_message hypercall input structure
 */
#define HV_MESSAGE_PAYLOAD_QWORD_COUNT  (30)

typedef struct {
	hv_vmbus_connection_id	connection_id;
	ULONG		        rsrvd;
	hv_vmbus_msg_type	message_type;
	ULONG		        payload_size;
	UQUAD		        payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
} hv_vmbus_input_post_message;

#if (0)
/* hv_vmbus_signal_event hypercall input structure */
typedef struct {
	hv_vmbus_connection_id	connection_id;
	UWORD		        flag_number;
	UWORD		        rsvd_z;
} __attribute__((packed)) hv_vmbus_input_signal_event;

typedef struct {
	UQUAD			        align8;
	hv_vmbus_input_signal_event     event;
} __attribute__((packed)) hv_vmbus_input_signal_event_buffer;
#endif

typedef enum {
	HV_VMBUS_PACKET_TYPE_INVALID				= 0x0,
	HV_VMBUS_PACKET_TYPES_SYNCH				= 0x1,
	HV_VMBUS_PACKET_TYPE_ADD_TRANSFER_PAGE_SET		= 0x2,
	HV_VMBUS_PACKET_TYPE_REMOVE_TRANSFER_PAGE_SET		= 0x3,
	HV_VMBUS_PACKET_TYPE_ESTABLISH_GPADL			= 0x4,
	HV_VMBUS_PACKET_TYPE_TEAR_DOWN_GPADL			= 0x5,
	HV_VMBUS_PACKET_TYPE_DATA_IN_BAND			= 0x6,
	HV_VMBUS_PACKET_TYPE_DATA_USING_TRANSFER_PAGES		= 0x7,
	HV_VMBUS_PACKET_TYPE_DATA_USING_GPADL			= 0x8,
	HV_VMBUS_PACKET_TYPE_DATA_USING_GPA_DIRECT		= 0x9,
	HV_VMBUS_PACKET_TYPE_CANCEL_REQUEST			= 0xa,
	HV_VMBUS_PACKET_TYPE_COMPLETION				= 0xb,
	HV_VMBUS_PACKET_TYPE_DATA_USING_ADDITIONAL_PACKETS	= 0xc,
	HV_VMBUS_PACKET_TYPE_ADDITIONAL_DATA = 0xd
} hv_vmbus_packet_type;

#define HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED    1

/*
 * Version 1 messages
 */
typedef enum {
	HV_CHANNEL_MESSAGE_INVALID			= 0,
	HV_CHANNEL_MESSAGE_OFFER_CHANNEL		= 1,
	HV_CHANNEL_MESSAGE_RESCIND_CHANNEL_OFFER	= 2,
	HV_CHANNEL_MESSAGE_REQUEST_OFFERS		= 3,
	HV_CHANNEL_MESSAGE_ALL_OFFERS_DELIVERED		= 4,
	HV_CHANNEL_MESSAGE_OPEN_CHANNEL			= 5,
	HV_CHANNEL_MESSAGE_OPEN_CHANNEL_RESULT		= 6,
	HV_CHANNEL_MESSAGE_CLOSE_CHANNEL		= 7,
	HV_CHANNEL_MESSAGEL_GPADL_HEADER		= 8,
	HV_CHANNEL_MESSAGE_GPADL_BODY			= 9,
	HV_CHANNEL_MESSAGE_GPADL_CREATED		= 10,
	HV_CHANNEL_MESSAGE_GPADL_TEARDOWN		= 11,
	HV_CHANNEL_MESSAGE_GPADL_TORNDOWN		= 12,
	HV_CHANNEL_MESSAGE_REL_ID_RELEASED		= 13,
	HV_CHANNEL_MESSAGE_INITIATED_CONTACT		= 14,
	HV_CHANNEL_MESSAGE_VERSION_RESPONSE		= 15,
	HV_CHANNEL_MESSAGE_UNLOAD			= 16,

#if (0)
#ifdef	HV_VMBUS_FEATURE_PARENT_OR_PEER_MEMORY_MAPPED_INTO_A_CHILD
	HV_CHANNEL_MESSAGE_VIEW_RANGE_ADD		= 17,
	HV_CHANNEL_MESSAGE_VIEW_RANGE_REMOVE		= 18,
#endif
#endif
	HV_CHANNEL_MESSAGE_COUNT
} hv_vmbus_channel_msg_type;

typedef struct {
	hv_vmbus_channel_msg_type	message_type;
	UWORD			        padding;
} __attribute__((packed)) hv_vmbus_channel_msg_header;

typedef struct {
	hv_vmbus_channel_msg_header     header;
	UWORD			        vmbus_version_requested;
	UWORD			        padding2;
	UQUAD			        interrupt_page;
	UQUAD			        monitor_page_1;
	UQUAD			        monitor_page_2;
} __attribute__((packed)) hv_vmbus_channel_initiate_contact;

struct vmbushv_staticdata
{
    struct Library                      *OOPBase;
    struct Library                      *utilityBase;
    APTR                                kernelBase;

    OOP_Object                          *hvbusObj;
    
    OOP_AttrBase                        hiddAB;
    OOP_AttrBase                        hwAttrBase;

    OOP_MethodID                        hwMethodBase;

    OOP_Class                           *vmbushvBusClass;
    OOP_Class                           *vmbushvDeviceClass;
};

#undef HiddAttrBase
#undef HWAttrBase
#undef HWBase

#define HiddAttrBase            (PSD(cl)->hiddAB)
#define HWAttrBase              (PSD(cl)->hwAttrBase)
#define HWBase                  (PSD(cl)->hwMethodBase)

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
    /* hypercall page */
    APTR                        vbhv_HCPage;
    APTR                        vbhv_HCAlloc;

    /* interrupt page */
    APTR                        vbhv_IPPage;
    APTR                        vbhv_IPAlloc;

    /* monitor pages */
    APTR                        vbhv_MPPage;
    APTR                        vbhv_MPAlloc;

    APTR                        vbhv_SynICMsgPage;
    APTR                        vbhv_SynICMsgAlloc;
    APTR                        vbhv_SynICEventPage;
    APTR                        vbhv_SynICEventAlloc;
};

struct VMBusHVDeviceData
{
    APTR                        vbdhv_void;
};

ULONG VMBusHV_DoChannelInitMsg(struct VMBusHVBusData *data);
ULONG VMBusHV_DoChannelOffersMsg(struct VMBusHVBusData *data);

#endif /* _VMBusHV_H */
