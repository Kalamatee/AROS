/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: x86-64 CPU frequency control
    Lang: english
*/

#define __KERNEL_NOLIBBASE__

#include <string.h>

#include <asm/cpu.h>

#include "kernel_base.h"
#include "kernel_debug.h"
#include "kernel_intern.h"

#define MSR_PLATFORM_INFO      0x000000CE
#define MSR_IA32_PERF_STATUS   0x00000198
#define MSR_IA32_PERF_CTL      0x00000199

#define CPUID_FEAT_EDX_MSR     (1U << 5)
#define CPUID_FEAT_ECX_EIST    (1U << 7)

#define PERF_CTL_RATIO_MASK    0x0000FF00ULL

static BOOL x86_64_is_intel(void)
{
    unsigned int eax, ebx, ecx, edx;
    char vendor[13];

    cpuid2(0, 0, &eax, &ebx, &ecx, &edx);
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';

    return strcmp(vendor, "GenuineIntel") == 0;
}

static BOOL x86_64_cpu_perf_init_core(struct PlatformData *pdata, apicid_t cpuNum)
{
    struct APICData *apicData = pdata->kb_APIC;
    struct CPUData *core;
    UQUAD platform_info;
    UQUAD perf_status;
    UBYTE max_ratio;
    UBYTE min_ratio;

    if (!apicData || cpuNum >= apicData->apic_count)
        return FALSE;

    core = &apicData->cores[cpuNum];
    if (core->cpu_PerfCapable)
        return TRUE;

    platform_info = rdmsrq(MSR_PLATFORM_INFO);
    max_ratio = (platform_info >> 8) & 0xff;
    min_ratio = (platform_info >> 40) & 0xff;

    if (!max_ratio)
        return FALSE;
    if (!min_ratio)
        min_ratio = max_ratio;

    perf_status = rdmsrq(MSR_IA32_PERF_STATUS);

    core->cpu_PerfMaxRatio = max_ratio;
    core->cpu_PerfMinRatio = min_ratio;
    core->cpu_PerfCurRatio = (perf_status >> 8) & 0xff;
    core->cpu_PerfCapable = 1;

    return TRUE;
}

static BOOL x86_64_CPUFreqSet(struct PlatformData *pdata, apicid_t cpuNum, UBYTE ratio)
{
    struct APICData *apicData;
    struct CPUData *core;
    UQUAD perf_ctl;

    if (!pdata || !(pdata->kb_PDFlags & PLATFORMF_CPUFREQ))
        return FALSE;

    if (!x86_64_cpu_perf_init_core(pdata, cpuNum))
        return FALSE;

    apicData = pdata->kb_APIC;
    core = &apicData->cores[cpuNum];

    if (ratio < core->cpu_PerfMinRatio)
        ratio = core->cpu_PerfMinRatio;
    if (ratio > core->cpu_PerfMaxRatio)
        ratio = core->cpu_PerfMaxRatio;

    perf_ctl = rdmsrq(MSR_IA32_PERF_CTL);
    perf_ctl = (perf_ctl & ~PERF_CTL_RATIO_MASK) | ((UQUAD)ratio << 8);
    wrmsrq(MSR_IA32_PERF_CTL, perf_ctl);

    return TRUE;
}

void core_CPUFreqInit(struct PlatformData *pdata)
{
    unsigned int eax, ebx, ecx, edx;

    if (!pdata)
        return;

    if (!x86_64_is_intel())
        return;

    cpuid2(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(edx & CPUID_FEAT_EDX_MSR))
        return;
    if (!(ecx & CPUID_FEAT_ECX_EIST))
        return;

    pdata->kb_CPUFreqSet = x86_64_CPUFreqSet;
    pdata->kb_CPUFreqPolicy.up_threshold = (ULONG)((70ULL << 32) / 100);
    pdata->kb_CPUFreqPolicy.down_threshold = (ULONG)((20ULL << 32) / 100);
    pdata->kb_PDFlags |= PLATFORMF_CPUFREQ;
}
