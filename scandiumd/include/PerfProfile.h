// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace scandium {

enum class Edition {
    ACADEMY,     // Efficient — low-end / battery saver
    CASUAL,      // Balanced — midrange daily driver
    PROFESSIONAL // Aggressive — high-end powerhouse
};

struct PerfProfile {
    // CPU
    int cpu_boost_ms;
    int sched_boost;           // 0=off 1=partial 2=full
    int top_app_cpuset;        // bitmask: big cores enabled
    bool sched_prefer_idle;
    int sched_uclamp_min;      // 0-1024

    // GPU
    int gpu_default_governor;  // 0=powersave 1=balanced 2=performance
    bool gpu_force_max_clock;
    int gpu_target_fps;

    // Memory
    int vm_swappiness;
    int vm_dirty_ratio;
    int vm_dirty_bg_ratio;
    int vm_vfs_cache_pressure;
    bool zram_enabled;
    int zram_comp_algo;        // 0=lz4 1=zstd 2=lzo-rle

    // I/O
    int io_scheduler;          // 0=noop 1=cfq 2=bfq 3=mq-deadline
    int io_read_ahead_kb;
    int io_nr_requests;

    // LMKD
    int lmk_minfree_adj;       // multiplier
    bool lmk_kill_heaviest;
    int lmk_thrashing_limit;

    // Thermal
    int thermal_throttle_temp;  // celsius
    bool thermal_adaptive;
};

Edition edition_from_string(const char* str);
const char* edition_to_string(Edition e);
PerfProfile get_profile(Edition edition);

} // namespace scandium
