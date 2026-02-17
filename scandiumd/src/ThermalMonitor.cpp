// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ThermalMonitor.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/properties.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

void ThermalMonitor::apply(const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: applying thermal config, threshold=" << profile.thermal_throttle_temp << "°C";
    set_throttle_threshold(profile.thermal_throttle_temp);

    if (profile.thermal_adaptive) {
        android::base::SetProperty("persist.scandium.thermal.adaptive", "true");
    } else {
        android::base::SetProperty("persist.scandium.thermal.adaptive", "false");
    }

    android::base::SetProperty("persist.scandium.thermal.threshold",
                               std::to_string(profile.thermal_throttle_temp));
}

int ThermalMonitor::read_cpu_temp() {
    const char* paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/devices/virtual/thermal/thermal_zone0/temp",
        nullptr
    };

    struct stat st;
    for (int i = 0; paths[i]; i++) {
        if (stat(paths[i], &st) != 0) continue;
        std::string temp_str;
        if (android::base::ReadFileToString(paths[i], &temp_str)) {
            int temp = std::stoi(temp_str);
            if (temp > 1000) temp /= 1000;
            return temp;
        }
    }
    return -1;
}

void ThermalMonitor::set_throttle_threshold(int temp_celsius) {
    // Set thermal mitigation threshold via property
    android::base::SetProperty("persist.sys.thermal.threshold", std::to_string(temp_celsius));

    // Try direct thermal zone trip point (may require kernel support)
    const char* trip_path = "/sys/class/thermal/thermal_zone0/trip_point_0_temp";
    struct stat st;
    if (stat(trip_path, &st) == 0) {
        // Write in millidegrees
        android::base::WriteStringToFile(
            std::to_string(temp_celsius * 1000), trip_path);
    }

    int current = read_cpu_temp();
    if (current >= 0) {
        LOG(INFO) << "scandiumd: current CPU temp=" << current
                  << "°C, throttle at " << temp_celsius << "°C";
    }
}

} // namespace scandium
