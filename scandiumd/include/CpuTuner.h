// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class CpuTuner {
public:
    static void apply(const PerfProfile& profile);

private:
    static void write_cpuset(const PerfProfile& profile);
    static void write_schedtune(const PerfProfile& profile);
    static void write_uclamp(const PerfProfile& profile);
    static void set_governor(const char* governor);
};

} // namespace scandium
