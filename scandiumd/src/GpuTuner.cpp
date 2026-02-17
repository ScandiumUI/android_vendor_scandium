// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "GpuTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

bool GpuTuner::write_sysfs(const char* path, const char* value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;

    if (!android::base::WriteStringToFile(std::string(value), path)) {
        LOG(WARNING) << "scandiumd: [GPU] write failed: " << path;
        return false;
    }
    return true;
}

void GpuTuner::apply(const GpuProfile& profile) {
    LOG(INFO) << "scandiumd: [GPU] applying tuning";

    set_adreno(profile);
    set_mali(profile);
    set_powervr(profile);
    set_generic(profile);

    LOG(INFO) << "scandiumd: [GPU] tuning complete";
}

void GpuTuner::set_adreno(const GpuProfile& profile) {
    const char* base = "/sys/class/kgsl/kgsl-3d0";
    struct stat st;
    if (stat(base, &st) != 0) return;

    LOG(INFO) << "scandiumd: [GPU] detected Qualcomm Adreno";

    const char* gov;
    switch (profile.governor) {
        case 0:  gov = "powersave";   break;
        case 2:  gov = "performance"; break;
        default: gov = "msm-adreno-tz"; break;
    }
    write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", gov);

    // Force max clock
    if (profile.force_max_clock) {
        std::string max_freq;
        if (android::base::ReadFileToString(
                "/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", &max_freq)) {
            max_freq.erase(max_freq.find_last_not_of("\n\r ") + 1);
            write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", max_freq.c_str());
            LOG(INFO) << "scandiumd: [GPU] adreno forced max clock=" << max_freq;
        }
    }

    // Throttling control
    write_sysfs("/sys/class/kgsl/kgsl-3d0/throttling",
        std::to_string(profile.gpu_throttle_level).c_str());

    // Bus control
    write_sysfs("/sys/class/kgsl/kgsl-3d0/bus_split",
        std::to_string(profile.bus_split).c_str());

    if (profile.force_bus_on) {
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_clk_on", "1");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_rail_on", "1");
    } else {
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "0");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_clk_on", "0");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/force_rail_on", "0");
    }

    // Idle timer
    write_sysfs("/sys/class/kgsl/kgsl-3d0/idle_timer",
        std::to_string(profile.idle_timer).c_str());

    // Adreno idler
    tune_adreno_idler(profile);

    // Power level control
    if (profile.governor == 2) {
        write_sysfs("/sys/class/kgsl/kgsl-3d0/default_pwrlevel", "0");
        write_sysfs("/sys/class/kgsl/kgsl-3d0/min_pwrlevel", "0");
    }

    // Adreno GPU boost if (profile.governor >= 1) {
        write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/adrenoboost", "2");
    }

    LOG(INFO) << "scandiumd: [GPU] adreno gov=" << gov
              << " throttle=" << profile.gpu_throttle_level
              << " idle_timer=" << profile.idle_timer;
}

void GpuTuner::tune_adreno_idler(const GpuProfile& profile) {
    const char* idler_base = "/sys/module/adreno_idler/parameters";
    struct stat st;
    if (stat(idler_base, &st) != 0) return;

    if (profile.adreno_idler_active) {
        write_sysfs("/sys/module/adreno_idler/parameters/adreno_idler_active", "Y");
        write_sysfs("/sys/module/adreno_idler/parameters/adreno_idler_idleworkload",
            std::to_string(profile.adreno_idler_delay).c_str());
        write_sysfs("/sys/module/adreno_idler/parameters/adreno_idler_downdifferential", "20");
    } else {
        write_sysfs("/sys/module/adreno_idler/parameters/adreno_idler_active", "N");
    }
}

void GpuTuner::set_mali(const GpuProfile& profile) {
    const char* paths[] = {
        "/sys/devices/platform/mali.0/devfreq/mali.0",
        "/sys/devices/platform/gpu/devfreq/gpu",
        "/sys/class/devfreq/13000000.mali",
        "/sys/class/devfreq/18500000.mali",
        "/sys/class/devfreq/mali0",
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

    LOG(INFO) << "scandiumd: [GPU] detected ARM Mali at " << found;
    tune_gpu_devfreq(found, profile);

    // Mali-specific power policy
    const char* mali_power_paths[] = {
        "/sys/devices/platform/mali.0/power_policy",
        "/sys/devices/platform/gpu/power_policy",
        nullptr
    };

    for (int i = 0; mali_power_paths[i]; i++) {
        if (stat(mali_power_paths[i], &st) == 0) {
            const char* policy = (profile.governor == 2) ? "always_on" : "demand";
            write_sysfs(mali_power_paths[i], policy);
        }
    }

    // Mali DVFS
    const char* dvfs_paths[] = {
        "/sys/devices/platform/mali.0/dvfs_enabled",
        "/sys/devices/platform/gpu/dvfs_enabled",
        nullptr
    };

    for (int i = 0; dvfs_paths[i]; i++) {
        if (stat(dvfs_paths[i], &st) == 0) {
            write_sysfs(dvfs_paths[i], (profile.governor == 2) ? "0" : "1");
        }
    }
}

void GpuTuner::set_powervr(const GpuProfile& profile) {
    struct stat st;
    if (stat("/sys/devices/platform/pvrsrvkm", &st) != 0) return;

    LOG(INFO) << "scandiumd: [GPU] detected PowerVR GPU";

    const char* devfreq_paths[] = {
        "/sys/class/devfreq/13040000.gpu",
        "/sys/class/devfreq/pvrsrvkm",
        nullptr
    };

    for (int i = 0; devfreq_paths[i]; i++) {
        if (stat(devfreq_paths[i], &st) == 0) {
            tune_gpu_devfreq(devfreq_paths[i], profile);
            break;
        }
    }
}

void GpuTuner::tune_gpu_devfreq(const char* base, const GpuProfile& profile) {
    std::string gov_path = std::string(base) + "/governor";
    const char* gov;
    switch (profile.governor) {
        case 0:  gov = "powersave";       break;
        case 2:  gov = "performance";     break;
        default: gov = "simple_ondemand"; break;
    }
    write_sysfs(gov_path.c_str(), gov);

    if (profile.force_max_clock) {
        std::string max_path = std::string(base) + "/max_freq";
        std::string min_path = std::string(base) + "/min_freq";
        std::string max_freq;
        if (android::base::ReadFileToString(max_path, &max_freq)) {
            max_freq.erase(max_freq.find_last_not_of("\n\r ") + 1);
            write_sysfs(min_path.c_str(), max_freq.c_str());
        }
    }

    // Polling interval
    std::string poll_path = std::string(base) + "/polling_interval";
    write_sysfs(poll_path.c_str(), (profile.governor == 2) ? "10" : "50");
}

void GpuTuner::set_generic(const GpuProfile& profile) {
    const char* paths[] = {
        "/sys/class/devfreq/gpufreq",
        "/sys/class/devfreq/5000000.qcom,kgsl-3d0",
        "/sys/class/devfreq/soc:qcom,gpubw",
        nullptr
    };

    struct stat st;
    for (int i = 0; paths[i]; i++) {
        if (stat(paths[i], &st) != 0) continue;
        tune_gpu_devfreq(paths[i], profile);
    }

    // GPU bandwidth control (Qualcomm)
    const char* bw_path = "/sys/class/devfreq/soc:qcom,gpubw/governor";
    if (stat(bw_path, &st) == 0) {
        const char* bw_gov = (profile.governor == 2) ? "performance" : "bw_vbif";
        write_sysfs(bw_path, bw_gov);
    }
}

} // namespace scandium
