// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "GpuTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

bool GpuTuner::write_sysfs(const char* path, const char* value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;

    if (!android::base::WriteStringToFile(std::string(value), path)) {
        LOG(WARNING) << "scandiumd: gpu write failed: " << path;
        return false;
    }
    return true;
}

void GpuTuner::apply(const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: applying GPU tuning";

    set_adreno(profile);
    set_mali(profile);
    set_generic(profile);
}

// Qualcomm Adreno GPU
void GpuTuner::set_adreno(const PerfProfile& profile) {
    const char* base = "/sys/class/kgsl/kgsl-3d0";
    struct stat st;
    if (stat(base, &st) != 0) return;

    LOG(INFO) << "scandiumd: detected Adreno GPU";

    // Governor
    const char* gov;
    switch (profile.gpu_default_governor) {
        case 0:  gov = "powersave";   break;
        case 2:  gov = "performance"; break;
        default: gov = "msm-adreno-tz"; break;
    }
    write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", gov);

    // Force max clock for Professional
    if (profile.gpu_force_max_clock) {
        std::string max_freq;
        if (android::base::ReadFileToString("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", &max_freq)) {
            write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", max_freq.c_str());
            LOG(INFO) << "scandiumd: adreno forced max clock=" << max_freq;
        }
    }

    // Adreno-specific tuning
    write_sysfs("/sys/class/kgsl/kgsl-3d0/throttling", "0");
    write_sysfs("/sys/class/kgsl/kgsl-3d0/bus_split", "0");

    if (profile.gpu_default_governor == 2) {
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_clk_on", "1");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_rail_on", "1");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/idle_timer", "10000");
    }
}

// ARM Mali GPU
void GpuTuner::set_mali(const PerfProfile& profile) {
    const char* paths[] = {
        "/sys/devices/platform/mali.0/devfreq/mali.0",
        "/sys/devices/platform/gpu/devfreq/gpu",
        "/sys/class/devfreq/13000000.mali",
        nullptr
    };

    const char* found = nullptr;
    struct stat st;
    for (int i = 0; paths[i]; i++) {
        if (stat(paths[i], &st) == 0) {
            found = paths[i];
            break;
        }
    }
    if (!found) return;

    LOG(INFO) << "scandiumd: detected Mali GPU at " << found;

    std::string gov_path = std::string(found) + "/governor";
    const char* gov;
    switch (profile.gpu_default_governor) {
        case 0:  gov = "powersave";    break;
        case 2:  gov = "performance";  break;
        default: gov = "simple_ondemand"; break;
    }
    write_sysfs(gov_path.c_str(), gov);

    if (profile.gpu_force_max_clock) {
        std::string max_path = std::string(found) + "/max_freq";
        std::string min_path = std::string(found) + "/min_freq";
        std::string max_freq;
        if (android::base::ReadFileToString(max_path, &max_freq)) {
            write_sysfs(min_path.c_str(), max_freq.c_str());
            LOG(INFO) << "scandiumd: mali forced max clock=" << max_freq;
        }
    }
}

// Generic fallback
void GpuTuner::set_generic(const PerfProfile& profile) {
    // Try generic GPU devfreq paths
    const char* paths[] = {
        "/sys/class/devfreq/gpufreq",
        "/sys/class/devfreq/5000000.qcom,kgsl-3d0",
        nullptr
    };

    struct stat st;
    for (int i = 0; paths[i]; i++) {
        if (stat(paths[i], &st) != 0) continue;

        std::string gov_path = std::string(paths[i]) + "/governor";
        const char* gov;
        switch (profile.gpu_default_governor) {
            case 0:  gov = "powersave";   break;
            case 2:  gov = "performance"; break;
            default: gov = "simple_ondemand"; break;
        }
        write_sysfs(gov_path.c_str(), gov);
    }
}

} // namespace scandium
