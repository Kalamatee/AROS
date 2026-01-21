/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: CPU frequency governor helpers for PC platforms
    Lang: english
*/

#include "cpufreq.h"

#define CPUFREQ_LOAD_HIGH ((ULONG)((70ULL << 32) / 100))
#define CPUFREQ_LOAD_LOW  ((ULONG)((20ULL << 32) / 100))

void core_CPUFreqUpdate(struct PlatformData *pdata, apicid_t cpuNum)
{
    struct APICData *apicData;
    struct CPUData *core;
    ULONG load;
    UBYTE target;

    if (!pdata || !(pdata->kb_PDFlags & PLATFORMF_CPUFREQ) || !pdata->kb_CPUFreqSet)
        return;

    apicData = pdata->kb_APIC;
    if (!apicData || cpuNum >= apicData->apic_count)
        return;

    core = &apicData->cores[cpuNum];

    load = core->cpu_Load;

    if (pdata->kb_CPUFreqPolicy.up_threshold == 0)
        pdata->kb_CPUFreqPolicy.up_threshold = CPUFREQ_LOAD_HIGH;
    if (pdata->kb_CPUFreqPolicy.down_threshold == 0)
        pdata->kb_CPUFreqPolicy.down_threshold = CPUFREQ_LOAD_LOW;

    if (load >= pdata->kb_CPUFreqPolicy.up_threshold)
        target = core->cpu_PerfMaxRatio ? core->cpu_PerfMaxRatio : 0xff;
    else if (load <= pdata->kb_CPUFreqPolicy.down_threshold)
        target = core->cpu_PerfMinRatio ? core->cpu_PerfMinRatio : 0x00;
    else
        return;

    if (target == core->cpu_PerfCurRatio)
        return;

    if (pdata->kb_CPUFreqSet(pdata, cpuNum, target))
    {
        if (load >= pdata->kb_CPUFreqPolicy.up_threshold && core->cpu_PerfMaxRatio)
            core->cpu_PerfCurRatio = core->cpu_PerfMaxRatio;
        else if (load <= pdata->kb_CPUFreqPolicy.down_threshold && core->cpu_PerfMinRatio)
            core->cpu_PerfCurRatio = core->cpu_PerfMinRatio;
        else
            core->cpu_PerfCurRatio = target;
    }
}
