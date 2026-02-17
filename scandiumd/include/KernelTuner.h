// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class KernelTuner {
public:
    static void apply(const KernelProfile& profile);

private:
    static void tune_printk(const KernelProfile& profile);
    static void tune_hung_task(const KernelProfile& profile);
    static void tune_panic(const KernelProfile& profile);
    static void tune_entropy(const KernelProfile& profile);
    static void tune_realtime(const KernelProfile& profile);
    static void tune_aslr(const KernelProfile& profile);
    static void tune_perf(const KernelProfile& profile);
    static bool write_proc_int(const char* path, int value);
};

} // namespace scandium
