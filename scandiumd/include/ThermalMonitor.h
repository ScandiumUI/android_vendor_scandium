// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class ThermalMonitor {
public:
    static void apply(const PerfProfile& profile);

private:
    static int read_cpu_temp();
    static void set_throttle_threshold(int temp_celsius);
};

} // namespace scandium
