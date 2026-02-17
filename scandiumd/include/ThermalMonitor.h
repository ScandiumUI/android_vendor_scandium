// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class ThermalMonitor {
public:
    static void apply(const ThermalProfile& profile);
    static int read_cpu_temp();
    static int read_gpu_temp();
    static int read_battery_temp();

private:
    static void set_throttle_threshold(int temp_celsius);
    static void configure_trip_points(const ThermalProfile& profile);
    static void configure_polling(const ThermalProfile& profile);
    static int read_thermal_zone(const char* zone);
    static int find_thermal_zone_count();
};

} // namespace scandium
