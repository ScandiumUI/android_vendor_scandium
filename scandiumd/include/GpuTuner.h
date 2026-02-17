// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class GpuTuner {
public:
    static void apply(const PerfProfile& profile);

private:
    static bool write_sysfs(const char* path, const char* value);
    static void set_adreno(const PerfProfile& profile);
    static void set_mali(const PerfProfile& profile);
    static void set_generic(const PerfProfile& profile);
};

} // namespace scandium
