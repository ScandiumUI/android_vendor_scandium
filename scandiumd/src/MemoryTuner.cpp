// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "MemoryTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>

#include <fstream>
#include <sstream>
#include <string>

namespace scandium {

static bool write_int(const char* path, int value) {
    return android::base::WriteStringToFile(std::to_string(value), path);
}

static bool write_str(const char* path, const std::string& value) {
    return android::base::WriteStringToFile(value, path);
}

void MemoryTuner::apply(const MemProfile& profile) {
    LOG(INFO) << "scandiumd: [MEM] applying memory tuning";

    tune_vm(profile);
    tune_zram(profile);
    tune_watermark(profile);
    tune_hugepages(profile);
    tune_ksm(profile);

    if (profile.compact_memory) {
        compact_memory();
    }

    LOG(INFO) << "scandiumd: [MEM] tuning complete";
}

void MemoryTuner::tune_vm(const MemProfile& profile) {
    write_int("/proc/sys/vm/swappiness", profile.swappiness);
    write_int("/proc/sys/vm/dirty_ratio", profile.dirty_ratio);
    write_int("/proc/sys/vm/dirty_background_ratio", profile.dirty_bg_ratio);
    write_int("/proc/sys/vm/vfs_cache_pressure", profile.vfs_cache_pressure);
    write_int("/proc/sys/vm/page-cluster", profile.page_cluster);
    write_int("/proc/sys/vm/dirty_expire_centisecs", profile.dirty_expire_cs);
    write_int("/proc/sys/vm/dirty_writeback_centisecs", profile.dirty_writeback_cs);
    write_int("/proc/sys/vm/overcommit_memory", profile.overcommit_memory);
    write_int("/proc/sys/vm/overcommit_ratio", profile.overcommit_ratio);
    write_int("/proc/sys/vm/min_free_kbytes", profile.min_free_kbytes);
    write_int("/proc/sys/vm/extra_free_kbytes", profile.extra_free_kbytes);
    write_int("/proc/sys/vm/oom_kill_allocating_task", profile.oom_kill_allocating_task);
    write_int("/proc/sys/vm/stat_interval", profile.stat_interval);

    // Additional VM tuning
    write_int("/proc/sys/vm/laptop_mode", 0);
    write_int("/proc/sys/vm/block_dump", 0);
    write_int("/proc/sys/vm/oom_dump_tasks", 0);
    write_int("/proc/sys/vm/page-cluster", profile.page_cluster);
    write_int("/proc/sys/vm/max_map_count", 262144);

    LOG(INFO) << "scandiumd: [MEM] vm: swappiness=" << profile.swappiness
              << " dirty=" << profile.dirty_ratio
              << " cache_pressure=" << profile.vfs_cache_pressure
              << " min_free=" << profile.min_free_kbytes << "kB";
}

void MemoryTuner::tune_zram(const MemProfile& profile) {
    if (!profile.zram_enabled) {
        write_str("/sys/block/zram0/reset", "1");
        LOG(INFO) << "scandiumd: [MEM] zram disabled";
        return;
    }

    const char* algo;
    switch (profile.zram_comp_algo) {
        case 1:  algo = "zstd";    break;
        case 2:  algo = "lzo-rle"; break;
        default: algo = "lz4";     break;
    }
    write_str("/sys/block/zram0/comp_algorithm", algo);
    write_int("/sys/block/zram0/max_comp_streams", profile.zram_max_comp_streams);

    LOG(INFO) << "scandiumd: [MEM] zram algo=" << algo
              << " streams=" << profile.zram_max_comp_streams;
}

void MemoryTuner::tune_watermark(const MemProfile& profile) {
    write_int("/proc/sys/vm/watermark_scale_factor", profile.watermark_scale_factor);
    write_int("/proc/sys/vm/watermark_boost_factor", 0);

    LOG(INFO) << "scandiumd: [MEM] watermark_scale_factor="
              << profile.watermark_scale_factor;
}

void MemoryTuner::tune_hugepages(const MemProfile& profile) {
    struct stat st;
    if (stat("/sys/kernel/mm/transparent_hugepage/enabled", &st) == 0) {
        if (profile.swappiness <= 10) {
            write_str("/sys/kernel/mm/transparent_hugepage/enabled", "always");
            write_str("/sys/kernel/mm/transparent_hugepage/defrag", "defer+madvise");
        } else {
            write_str("/sys/kernel/mm/transparent_hugepage/enabled", "madvise");
            write_str("/sys/kernel/mm/transparent_hugepage/defrag", "madvise");
        }
    }
}

void MemoryTuner::tune_ksm(const MemProfile& profile) {
    struct stat st;
    if (stat("/sys/kernel/mm/ksm/run", &st) != 0) return;

    if (profile.swappiness >= 80) {
        write_int("/sys/kernel/mm/ksm/run", 1);
        write_int("/sys/kernel/mm/ksm/sleep_millisecs", 500);
        write_int("/sys/kernel/mm/ksm/pages_to_scan", 100);
        LOG(INFO) << "scandiumd: [MEM] KSM enabled (memory saving mode)";
    } else {
        write_int("/sys/kernel/mm/ksm/run", 0);
        LOG(INFO) << "scandiumd: [MEM] KSM disabled";
    }
}

void MemoryTuner::drop_caches() {
    write_str("/proc/sys/vm/drop_caches", "3");
    LOG(INFO) << "scandiumd: [MEM] caches dropped";
}

void MemoryTuner::compact_memory() {
    struct stat st;
    if (stat("/proc/sys/vm/compact_memory", &st) == 0) {
        write_str("/proc/sys/vm/compact_memory", "1");
        LOG(INFO) << "scandiumd: [MEM] memory compaction triggered";
    }
}

long MemoryTuner::get_available_ram_kb() {
    return get_meminfo_field("MemAvailable");
}

long MemoryTuner::get_meminfo_field(const char* field) {
    std::string meminfo;
    if (!android::base::ReadFileToString("/proc/meminfo", &meminfo)) return -1;

    std::istringstream iss(meminfo);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find(field) == 0) {
            std::istringstream ls(line);
            std::string key;
            long value;
            ls >> key >> value;
            return value;
        }
    }
    return -1;
}

} // namespace scandium
