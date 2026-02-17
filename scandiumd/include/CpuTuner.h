// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class CpuTuner {
public:
    static void apply(const CpuProfile& profile);
    static void apply_boost(int duration_ms);
    static void reset_boost();

private:
    static void write_cpuset(const CpuProfile& profile);
    static void write_schedtune(const CpuProfile& profile);
    static void write_uclamp(const CpuProfile& profile);
    static void set_governor(const char* governor);
    static void set_frequency_limits(const CpuProfile& profile);
    static void configure_eas(const CpuProfile& profile);
    static int detect_core_count();
    static void detect_cluster_layout();
};

} // namespace scandium
