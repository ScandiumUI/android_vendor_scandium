// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "CpuTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <dirent.h>
#include <string>

namespace scandium {

static bool write_node(const char* path, const std::string& value) {
    if (!android::base::WriteStringToFile(value, path)) {
        LOG(WARNING) << "scandiumd: failed to write " << value << " -> " << path;
        return false;
    }
    return true;
}

void CpuTuner::apply(const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: applying CPU tuning";
    write_cpuset(profile);
    write_schedtune(profile);
    write_uclamp(profile);

    // Set governor based on edition
    switch (profile.sched_boost) {
        case 0: set_governor("powersave");    break;
        case 1: set_governor("schedutil");    break;
        case 2: set_governor("performance");  break;
    }
}

void CpuTuner::write_cpuset(const PerfProfile& profile) {
    // Build cpuset string from bitmask
    std::string cpus;
    for (int i = 0; i < 8; i++) {
        if (profile.top_app_cpuset & (1 << i)) {
            if (!cpus.empty()) cpus += ",";
            cpus += std::to_string(i);
        }
    }

    write_node("/dev/cpuset/top-app/cpus", cpus);
    LOG(INFO) << "scandiumd: cpuset top-app=" << cpus;

    // Foreground gets same or slightly less
    write_node("/dev/cpuset/foreground/cpus", cpus);

    // Background always on little cores
    write_node("/dev/cpuset/background/cpus", "0-3");
    write_node("/dev/cpuset/system-background/cpus", "0-3");
}

void CpuTuner::write_schedtune(const PerfProfile& profile) {
    // SchedTune boost for top-app
    std::string boost = std::to_string(profile.sched_boost * 5); // 0, 5, 10
    write_node("/dev/stune/top-app/schedtune.boost", boost);

    std::string prefer = profile.sched_prefer_idle ? "1" : "0";
    write_node("/dev/stune/top-app/schedtune.prefer_idle", prefer);

    // Background: never boost
    write_node("/dev/stune/background/schedtune.boost", "0");
    write_node("/dev/stune/background/schedtune.prefer_idle", "1");
}

void CpuTuner::write_uclamp(const PerfProfile& profile) {
    std::string uclamp_min = std::to_string(profile.sched_uclamp_min);
    write_node("/proc/sys/kernel/sched_util_clamp_min_rt_default", uclamp_min);

    // Top-app uclamp
    std::string path = "/dev/cpuctl/top-app/cpu.uclamp.min";
    write_node(path.c_str(), uclamp_min);

    // Background: minimal
    write_node("/dev/cpuctl/background/cpu.uclamp.min", "0");
    write_node("/dev/cpuctl/background/cpu.uclamp.max", "384");
}

void CpuTuner::set_governor(const char* governor) {
    DIR* dir = opendir("/sys/devices/system/cpu");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.find("cpu") != 0 || name.find("cpufreq") != std::string::npos) continue;
        if (name.length() < 4 || !isdigit(name[3])) continue;

        std::string path = android::base::StringPrintf(
            "/sys/devices/system/cpu/%s/cpufreq/scaling_governor", name.c_str());
        write_node(path.c_str(), governor);
    }
    closedir(dir);

    LOG(INFO) << "scandiumd: cpu governor=" << governor;
}

} // namespace scandium
