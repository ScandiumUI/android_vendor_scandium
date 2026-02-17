// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class GpuTuner {
public:
    static void apply(const GpuProfile& profile);

private:
    static bool write_sysfs(const char* path, const char* value);
    static void set_adreno(const GpuProfile& profile);
    static void set_mali(const GpuProfile& profile);
    static void set_powervr(const GpuProfile& profile);
    static void set_generic(const GpuProfile& profile);
    static void tune_adreno_idler(const GpuProfile& profile);
    static void tune_gpu_devfreq(const char* base, const GpuProfile& profile);
};

} // namespace scandium
