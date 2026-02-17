// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "FsOptimizer.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <string>

namespace scandium {

bool FsOptimizer::write_sysfs(const char* path, const std::string& value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return android::base::WriteStringToFile(value, path);
}

void FsOptimizer::apply(const FsProfile& profile) {
    LOG(INFO) << "scandiumd: [FS] applying filesystem tuning";

    if (has_f2fs()) {
        tune_f2fs(profile);
    }

    if (has_ext4()) {
        tune_ext4(profile);
    }

    tune_inotify_limits(profile);
    tune_dentry_cache();

    LOG(INFO) << "scandiumd: [FS] tuning complete";
}

void FsOptimizer::tune_f2fs(const FsProfile& profile) {
    LOG(INFO) << "scandiumd: [FS] detected F2FS, tuning...";

    // F2FS GC tuning
    const char* f2fs_paths[] = {
        "/sys/fs/f2fs/sda1",
        "/sys/fs/f2fs/dm-0",
        "/sys/fs/f2fs/mmcblk0p1",
        nullptr
    };

    struct stat st;
    for (int i = 0; f2fs_paths[i]; i++) {
        if (stat(f2fs_paths[i], &st) != 0) continue;

        std::string base = f2fs_paths[i];

        // GC control
        std::string gc_path = base + "/gc_urgent";
        if (profile.f2fs_gc_enable) {
            write_sysfs(gc_path.c_str(), "1");
        } else {
            write_sysfs(gc_path.c_str(), "0");
        }

        write_sysfs((base + "/gc_urgent_sleep_time").c_str(),
            std::to_string(profile.f2fs_gc_urgent_sleep_time));
        write_sysfs((base + "/gc_min_sleep_time").c_str(),
            std::to_string(profile.f2fs_gc_min_sleep_time));
        write_sysfs((base + "/gc_max_sleep_time").c_str(),
            std::to_string(profile.f2fs_gc_max_sleep_time));

        // I/O stats
        write_sysfs((base + "/iostat_enable").c_str(),
            profile.f2fs_iostat_enable ? "1" : "0");

        // Extent cache
        write_sysfs((base + "/max_small_discards").c_str(), "512");

        // RAM threshold
        write_sysfs((base + "/ram_thresh").c_str(), "60");

        // Trim
        write_sysfs((base + "/trim_sections").c_str(), "32");

        LOG(INFO) << "scandiumd: [FS] F2FS " << base
                  << " gc=" << profile.f2fs_gc_enable
                  << " gc_sleep=" << profile.f2fs_gc_min_sleep_time;
    }
}

void FsOptimizer::tune_ext4(const FsProfile& profile) {
    LOG(INFO) << "scandiumd: [FS] EXT4 tuning (via mount options not applicable at runtime)";

    // EXT4 parameters that can be tuned at runtime
    struct stat st;
    if (stat("/proc/sys/fs/dir-notify-enable", &st) == 0) {
        write_sysfs("/proc/sys/fs/dir-notify-enable", "0");
    }
}

void FsOptimizer::tune_inotify_limits(const FsProfile& profile) {
    android::base::WriteStringToFile(
        std::to_string(profile.inotify_max_user_watches),
        "/proc/sys/fs/inotify/max_user_watches");
    android::base::WriteStringToFile(
        std::to_string(profile.inotify_max_queued_events),
        "/proc/sys/fs/inotify/max_queued_events");

    // Also tune max user instances
    android::base::WriteStringToFile("512",
        "/proc/sys/fs/inotify/max_user_instances");

    // File descriptor limits
    android::base::WriteStringToFile("2097152",
        "/proc/sys/fs/file-max");
    android::base::WriteStringToFile("1048576",
        "/proc/sys/fs/nr_open");

    // Pipe limits
    android::base::WriteStringToFile("1048576",
        "/proc/sys/fs/pipe-max-size");

    // AIO limits
    android::base::WriteStringToFile("1048576",
        "/proc/sys/fs/aio-max-nr");

    // Dentry negative lookups
    android::base::WriteStringToFile("60",
        "/proc/sys/fs/lease-break-time");

    LOG(INFO) << "scandiumd: [FS] inotify watches=" << profile.inotify_max_user_watches
              << " events=" << profile.inotify_max_queued_events;
}

void FsOptimizer::tune_dentry_cache() {
    // Protect dentries from being evicted too aggressively
    struct stat st;
    if (stat("/proc/sys/fs/vfs_cache_pressure", &st) == 0) {
        // Already tuned by MemoryTuner, skip here
    }
}

void FsOptimizer::detect_filesystem_type(std::string& fs_type, std::string& mount_point) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find("/data") != std::string::npos) {
            std::istringstream iss(line);
            std::string dev, mp, type;
            iss >> dev >> mp >> type;
            fs_type = type;
            mount_point = mp;
            return;
        }
    }
    fs_type = "unknown";
    mount_point = "/data";
}

bool FsOptimizer::has_f2fs() {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find("f2fs") != std::string::npos) return true;
    }
    return false;
}

bool FsOptimizer::has_ext4() {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find("ext4") != std::string::npos &&
            line.find("/data") != std::string::npos) return true;
    }
    return false;
}

} // namespace scandium
