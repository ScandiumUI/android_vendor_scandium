// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ThermalMonitor.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

void ThermalMonitor::apply(const ThermalProfile& profile) {
    LOG(INFO) << "scandiumd: [THERMAL] applying config, threshold="
              << profile.throttle_temp << "°C";

    set_throttle_threshold(profile.throttle_temp);
    configure_trip_points(profile);
    configure_polling(profile);

    android::base::SetProperty("persist.scandium.thermal.adaptive",
        profile.adaptive ? "true" : "false");
    android::base::SetProperty("persist.scandium.thermal.threshold",
        std::to_string(profile.throttle_temp));

    int current = read_cpu_temp();
    if (current >= 0) {
        LOG(INFO) << "scandiumd: [THERMAL] current CPU temp=" << current
                  << "°C, throttle at " << profile.throttle_temp << "°C";
    }

    int gpu_temp = read_gpu_temp();
    if (gpu_temp >= 0) {
        LOG(INFO) << "scandiumd: [THERMAL] current GPU temp=" << gpu_temp << "°C";
    }

    int batt_temp = read_battery_temp();
    if (batt_temp >= 0) {
        LOG(INFO) << "scandiumd: [THERMAL] battery temp=" << batt_temp << "°C";
    }

    LOG(INFO) << "scandiumd: [THERMAL] config complete";
}

void ThermalMonitor::set_throttle_threshold(int temp_celsius) {
    android::base::SetProperty("persist.sys.thermal.threshold",
        std::to_string(temp_celsius));

    const char* trip_path = "/sys/class/thermal/thermal_zone0/trip_point_0_temp";
    struct stat st;
    if (stat(trip_path, &st) == 0) {
        android::base::WriteStringToFile(
            std::to_string(temp_celsius * 1000), trip_path);
    }
}

void ThermalMonitor::configure_trip_points(const ThermalProfile& profile) {
    int zone_count = find_thermal_zone_count();

    for (int z = 0; z < zone_count; z++) {
        std::string trip0 = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/trip_point_0_temp", z);
        std::string trip1 = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/trip_point_1_temp", z);
        std::string trip2 = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/trip_point_2_temp", z);

        struct stat st;
        if (stat(trip0.c_str(), &st) == 0) {
            android::base::WriteStringToFile(
                std::to_string(profile.trip_point_0), trip0);
        }
        if (stat(trip1.c_str(), &st) == 0) {
            android::base::WriteStringToFile(
                std::to_string(profile.trip_point_1), trip1);
        }
        if (stat(trip2.c_str(), &st) == 0) {
            android::base::WriteStringToFile(
                std::to_string(profile.trip_point_2), trip2);
        }
    }

    LOG(INFO) << "scandiumd: [THERMAL] trip points: "
              << profile.trip_point_0 / 1000 << "°C / "
              << profile.trip_point_1 / 1000 << "°C / "
              << profile.trip_point_2 / 1000 << "°C (across "
              << zone_count << " zones)";
}

void ThermalMonitor::configure_polling(const ThermalProfile& profile) {
    int zone_count = find_thermal_zone_count();

    for (int z = 0; z < zone_count; z++) {
        std::string poll_path = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/polling_delay", z);
        struct stat st;
        if (stat(poll_path.c_str(), &st) == 0) {
            android::base::WriteStringToFile(
                std::to_string(profile.poll_interval_ms), poll_path);
        }
    }
}

int ThermalMonitor::read_cpu_temp() {
    // Try multiple thermal zones for CPU
    for (int z = 0; z < 20; z++) {
        std::string type_path = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/type", z);
        std::string type;
        if (!android::base::ReadFileToString(type_path, &type)) break;

        type.erase(type.find_last_not_of("\n\r ") + 1);

        // Common CPU thermal zone names
        if (type.find("cpu") != std::string::npos ||
            type.find("CPU") != std::string::npos ||
            type.find("tsens_tz_sensor") != std::string::npos ||
            type == "soc_thermal" || type == "mtktscpu" ||
            z == 0) {
            int temp = read_thermal_zone(
                android::base::StringPrintf(
                    "/sys/class/thermal/thermal_zone%d/temp", z).c_str());
            if (temp >= 0) return temp;
        }
    }

    return read_thermal_zone("/sys/class/thermal/thermal_zone0/temp");
}

int ThermalMonitor::read_gpu_temp() {
    for (int z = 0; z < 20; z++) {
        std::string type_path = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/type", z);
        std::string type;
        if (!android::base::ReadFileToString(type_path, &type)) break;

        type.erase(type.find_last_not_of("\n\r ") + 1);

        if (type.find("gpu") != std::string::npos ||
            type.find("GPU") != std::string::npos) {
            return read_thermal_zone(
                android::base::StringPrintf(
                    "/sys/class/thermal/thermal_zone%d/temp", z).c_str());
        }
    }
    return -1;
}

int ThermalMonitor::read_battery_temp() {
    // Try battery thermal zone
    for (int z = 0; z < 20; z++) {
        std::string type_path = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d/type", z);
        std::string type;
        if (!android::base::ReadFileToString(type_path, &type)) break;

        type.erase(type.find_last_not_of("\n\r ") + 1);

        if (type.find("battery") != std::string::npos ||
            type.find("Battery") != std::string::npos) {
            return read_thermal_zone(
                android::base::StringPrintf(
                    "/sys/class/thermal/thermal_zone%d/temp", z).c_str());
        }
    }

    // Fallback: power supply temp
    std::string temp_str;
    if (android::base::ReadFileToString(
            "/sys/class/power_supply/battery/temp", &temp_str)) {
        int temp = std::stoi(temp_str);
        return temp / 10;
    }

    return -1;
}

int ThermalMonitor::read_thermal_zone(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    std::string temp_str;
    if (android::base::ReadFileToString(path, &temp_str)) {
        int temp = std::stoi(temp_str);
        if (temp > 1000) temp /= 1000;
        return temp;
    }
    return -1;
}

int ThermalMonitor::find_thermal_zone_count() {
    int count = 0;
    for (int z = 0; z < 30; z++) {
        std::string path = android::base::StringPrintf(
            "/sys/class/thermal/thermal_zone%d", z);
        struct stat st;
        if (stat(path.c_str(), &st) != 0) break;
        count++;
    }
    return count;
}

} // namespace scandium
