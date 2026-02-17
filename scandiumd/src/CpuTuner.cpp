// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "CpuTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/properties.h>

#include <dirent.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <string>

namespace scandium {

static bool write_node(const char* path, const std::string& value) {
    if (!android::base::WriteStringToFile(value, path)) {
        LOG(WARNING) << "scandiumd: cpu write failed: " << value << " -> " << path;
        return false;
    }
    return true;
}

void CpuTuner::apply(const CpuProfile& profile) {
    LOG(INFO) << "scandiumd: [CPU] applying tuning";

    int cores = detect_core_count();
    LOG(INFO) << "scandiumd: [CPU] detected " << cores << " cores";

    write_cpuset(profile);
    write_schedtune(profile);
    write_uclamp(profile);
    configure_eas(profile);
    set_frequency_limits(profile);

    switch (profile.sched_boost) {
        case 0: set_governor("powersave");    break;
        case 1: set_governor("schedutil");    break;
        case 2: set_governor("performance");  break;
    }

    write_node("/proc/sys/kernel/sched_nr_migrate",
        std::to_string(profile.sched_nr_migrate));
    write_node("/proc/sys/kernel/sched_migration_cost_ns",
        std::to_string(profile.sched_migration_cost_ns));

    LOG(INFO) << "scandiumd: [CPU] tuning complete";
}

void CpuTuner::apply_boost(int duration_ms) {
    if (duration_ms <= 0) return;

    LOG(INFO) << "scandiumd: [CPU] applying boost for " << duration_ms << "ms";

    write_node("/sys/module/cpu_boost/parameters/input_boost_enabled", "1");
    write_node("/sys/module/cpu_boost/parameters/input_boost_ms",
        std::to_string(duration_ms));
    write_node("/dev/stune/top-app/schedtune.boost", "15");
    android::base::SetProperty("persist.scandium.cpu.boost_active", "true");
}

void CpuTuner::reset_boost() {
    write_node("/dev/stune/top-app/schedtune.boost", "5");
    write_node("/sys/module/cpu_boost/parameters/input_boost_enabled", "0");
    android::base::SetProperty("persist.scandium.cpu.boost_active", "false");
}

void CpuTuner::write_cpuset(const CpuProfile& profile) {
    auto mask_to_cpus = [](int mask) -> std::string {
        std::string cpus;
        for (int i = 0; i < 8; i++) {
            if (mask & (1 << i)) {
                if (!cpus.empty()) cpus += ",";
                cpus += std::to_string(i);
            }
        }
        return cpus;
    };

    std::string top_cpus = mask_to_cpus(profile.top_app_cpuset);
    std::string fg_cpus = mask_to_cpus(profile.foreground_cpuset);
    std::string bg_cpus = mask_to_cpus(profile.background_cpuset);

    write_node("/dev/cpuset/top-app/cpus", top_cpus);
    write_node("/dev/cpuset/foreground/cpus", fg_cpus);
    write_node("/dev/cpuset/background/cpus", bg_cpus);
    write_node("/dev/cpuset/system-background/cpus", bg_cpus);
    write_node("/dev/cpuset/restricted/cpus", bg_cpus);
    write_node("/dev/cpuset/top-app/mems", "0");
    write_node("/dev/cpuset/foreground/mems", "0");
    write_node("/dev/cpuset/background/mems", "0");

    LOG(INFO) << "scandiumd: [CPU] cpuset top=" << top_cpus
              << " fg=" << fg_cpus << " bg=" << bg_cpus;
}

void CpuTuner::write_schedtune(const CpuProfile& profile) {
    int boost = profile.sched_boost * 5;
    write_node("/dev/stune/top-app/schedtune.boost", std::to_string(boost));
    write_node("/dev/stune/foreground/schedtune.boost", std::to_string(boost / 2));
    write_node("/dev/stune/background/schedtune.boost", "0");
    write_node("/dev/stune/rt/schedtune.boost", std::to_string(boost));

    std::string prefer = profile.sched_prefer_idle ? "1" : "0";
    write_node("/dev/stune/top-app/schedtune.prefer_idle", prefer);
    write_node("/dev/stune/foreground/schedtune.prefer_idle", prefer);
    write_node("/dev/stune/background/schedtune.prefer_idle", "1");

    LOG(INFO) << "scandiumd: [CPU] schedtune boost=" << boost
              << " prefer_idle=" << profile.sched_prefer_idle;
}

void CpuTuner::write_uclamp(const CpuProfile& profile) {
    write_node("/dev/cpuctl/top-app/cpu.uclamp.min",
        std::to_string(profile.sched_uclamp_min_top_app));
    write_node("/dev/cpuctl/top-app/cpu.uclamp.max",
        std::to_string(profile.sched_uclamp_max_top_app));
    write_node("/dev/cpuctl/foreground/cpu.uclamp.min",
        std::to_string(profile.sched_uclamp_min));
    write_node("/dev/cpuctl/foreground/cpu.uclamp.max",
        std::to_string(profile.sched_uclamp_max));
    write_node("/dev/cpuctl/background/cpu.uclamp.min", "0");
    write_node("/dev/cpuctl/background/cpu.uclamp.max", "384");
    write_node("/dev/cpuctl/system-background/cpu.uclamp.min", "0");
    write_node("/dev/cpuctl/system-background/cpu.uclamp.max", "512");
    write_node("/proc/sys/kernel/sched_util_clamp_min_rt_default",
        std::to_string(profile.sched_uclamp_min));

    LOG(INFO) << "scandiumd: [CPU] uclamp top_app min="
              << profile.sched_uclamp_min_top_app
              << " max=" << profile.sched_uclamp_max_top_app;
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
    write_node("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor", governor);

    LOG(INFO) << "scandiumd: [CPU] governor=" << governor;
}

void CpuTuner::set_frequency_limits(const CpuProfile& profile) {
    if (profile.sched_boost < 2) return;

    const char* policies[] = {
        "/sys/devices/system/cpu/cpufreq/policy0",
        "/sys/devices/system/cpu/cpufreq/policy4",
        "/sys/devices/system/cpu/cpufreq/policy6",
        "/sys/devices/system/cpu/cpufreq/policy7",
        nullptr
    };

    struct stat st;
    for (int i = 0; policies[i]; i++) {
        if (stat(policies[i], &st) != 0) continue;

        std::string max_freq_path = std::string(policies[i]) + "/cpuinfo_max_freq";
        std::string max_freq;
        if (android::base::ReadFileToString(max_freq_path, &max_freq)) {
            max_freq.erase(max_freq.find_last_not_of("\n\r ") + 1);
            std::string hispeed_path = std::string(policies[i]) + "/schedutil/hispeed_freq";
            write_node(hispeed_path.c_str(), max_freq);
            std::string hispeed_load = std::string(policies[i]) + "/schedutil/hispeed_load";
            write_node(hispeed_load.c_str(), "60");
        }
    }
}

void CpuTuner::configure_eas(const CpuProfile& profile) {
    if (profile.energy_aware) {
        write_node("/proc/sys/kernel/sched_energy_aware", "1");
        LOG(INFO) << "scandiumd: [CPU] EAS enabled";
    } else {
        write_node("/proc/sys/kernel/sched_energy_aware", "0");
        LOG(INFO) << "scandiumd: [CPU] EAS disabled (max performance)";
    }
}

int CpuTuner::detect_core_count() {
    int count = 0;
    DIR* dir = opendir("/sys/devices/system/cpu");
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.find("cpu") == 0 && name.length() >= 4 && isdigit(name[3]))
            count++;
    }
    closedir(dir);
    return count;
}

void CpuTuner::detect_cluster_layout() {
    LOG(INFO) << "scandiumd: [CPU] detecting cluster layout...";

    const char* policies[] = {
        "/sys/devices/system/cpu/cpufreq/policy0",
        "/sys/devices/system/cpu/cpufreq/policy4",
        "/sys/devices/system/cpu/cpufreq/policy6",
        "/sys/devices/system/cpu/cpufreq/policy7",
        nullptr
    };

    struct stat st;
    int cluster = 0;
    for (int i = 0; policies[i]; i++) {
        if (stat(policies[i], &st) != 0) continue;

        std::string cpus_path = std::string(policies[i]) + "/related_cpus";
        std::string cpus;
        if (android::base::ReadFileToString(cpus_path, &cpus)) {
            cpus.erase(cpus.find_last_not_of("\n\r ") + 1);

            std::string max_path = std::string(policies[i]) + "/cpuinfo_max_freq";
            std::string max_freq;
            android::base::ReadFileToString(max_path, &max_freq);
            max_freq.erase(max_freq.find_last_not_of("\n\r ") + 1);

            LOG(INFO) << "scandiumd: [CPU] cluster " << cluster
                      << ": cpus=[" << cpus << "] max_freq=" << max_freq << " kHz";
            cluster++;
        }
    }
}

} // namespace scandium
