// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "MemoryTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>

#include <string>

namespace scandium {

static bool write_node(const char* path, int value) {
    return android::base::WriteStringToFile(std::to_string(value), path);
}

void MemoryTuner::apply(const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: applying memory tuning";
    tune_vm(profile);
    tune_zram(profile);
}

void MemoryTuner::tune_vm(const PerfProfile& profile) {
    write_node("/proc/sys/vm/swappiness", profile.vm_swappiness);
    write_node("/proc/sys/vm/dirty_ratio", profile.vm_dirty_ratio);
    write_node("/proc/sys/vm/dirty_background_ratio", profile.vm_dirty_bg_ratio);
    write_node("/proc/sys/vm/vfs_cache_pressure", profile.vm_vfs_cache_pressure);

    // Extra memory tuning
    write_node("/proc/sys/vm/page-cluster", profile.vm_swappiness > 50 ? 3 : 0);
    write_node("/proc/sys/vm/dirty_expire_centisecs",
               profile.vm_dirty_ratio > 30 ? 3000 : 1000);
    write_node("/proc/sys/vm/dirty_writeback_centisecs",
               profile.vm_dirty_ratio > 30 ? 3000 : 500);

    // Overcommit: allow more on Professional, strict on Academy
    if (profile.vm_swappiness <= 10) {
        write_node("/proc/sys/vm/overcommit_memory", 1);
    }

    LOG(INFO) << "scandiumd: vm swappiness=" << profile.vm_swappiness
              << " dirty=" << profile.vm_dirty_ratio
              << " cache_pressure=" << profile.vm_vfs_cache_pressure;
}

void MemoryTuner::tune_zram(const PerfProfile& profile) {
    if (!profile.zram_enabled) {
        // Disable zram for Professional — prefer raw RAM speed
        android::base::WriteStringToFile("1", "/sys/block/zram0/reset");
        LOG(INFO) << "scandiumd: zram disabled";
        return;
    }

    const char* algo;
    switch (profile.zram_comp_algo) {
        case 1:  algo = "zstd";    break;
        case 2:  algo = "lzo-rle"; break;
        default: algo = "lz4";     break;
    }
    android::base::WriteStringToFile(algo, "/sys/block/zram0/comp_algorithm");
    LOG(INFO) << "scandiumd: zram algo=" << algo;
}

void MemoryTuner::drop_caches() {
    android::base::WriteStringToFile("3", "/proc/sys/vm/drop_caches");
}

} // namespace scandium
