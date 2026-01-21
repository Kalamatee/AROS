#ifndef KERNEL_CPUFREQ_H
#define KERNEL_CPUFREQ_H
/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Desc: CPU frequency governor helpers for PC platforms
    Lang: english
*/

#include "apic.h"
#include "kernel_arch.h"

void core_CPUFreqUpdate(struct PlatformData *pdata, apicid_t cpuNum);

#endif /* KERNEL_CPUFREQ_H */
