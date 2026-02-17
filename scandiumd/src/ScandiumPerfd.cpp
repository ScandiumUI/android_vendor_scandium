// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ScandiumPerfd.h"
#include "CpuTuner.h"
#include "GpuTuner.h"
#include "MemoryTuner.h"
#include "IoTuner.h"
#include "GpuSpoof.h"
#include "ThermalMonitor.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

namespace scandium {

ScandiumPerfd::ScandiumPerfd()
    : mEdition(detect_edition()),
      mProfile(get_profile(mEdition)) {
}

Edition ScandiumPerfd::detect_edition() {
    std::string level = android::base::GetProperty("persist.scandium.perf.level", "balanced");

    if (level == "aggressive") return Edition::PROFESSIONAL;
    if (level == "efficient")  return Edition::ACADEMY;
    return Edition::CASUAL;
}

int ScandiumPerfd::run() {
    LOG(INFO) << "scandiumd: edition=" << edition_to_string(mEdition);
    log_profile();
    apply_profile();
    LOG(INFO) << "scandiumd: all profiles applied";
    return 0;
}

void ScandiumPerfd::apply_profile() {
    CpuTuner::apply(mProfile);
    GpuTuner::apply(mProfile);
    MemoryTuner::apply(mProfile);
    IoTuner::apply(mProfile);
    GpuSpoof::apply(mEdition);
    ThermalMonitor::apply(mProfile);
}

void ScandiumPerfd::log_profile() {
    LOG(INFO) << "scandiumd: cpu_boost=" << mProfile.cpu_boost_ms
              << " sched_boost=" << mProfile.sched_boost
              << " uclamp_min=" << mProfile.sched_uclamp_min;
    LOG(INFO) << "scandiumd: gpu_governor=" << mProfile.gpu_default_governor
              << " gpu_force_max=" << mProfile.gpu_force_max_clock
              << " gpu_fps=" << mProfile.gpu_target_fps;
    LOG(INFO) << "scandiumd: swappiness=" << mProfile.vm_swappiness
              << " dirty_ratio=" << mProfile.vm_dirty_ratio
              << " vfs_cache=" << mProfile.vm_vfs_cache_pressure;
    LOG(INFO) << "scandiumd: io_read_ahead=" << mProfile.io_read_ahead_kb
              << " io_nr_req=" << mProfile.io_nr_requests;
}

// --- Profile definitions ---

Edition edition_from_string(const char* str) {
    if (!str) return Edition::CASUAL;
    std::string s(str);
    if (s == "professional" || s == "aggressive") return Edition::PROFESSIONAL;
    if (s == "academy" || s == "efficient")       return Edition::ACADEMY;
    return Edition::CASUAL;
}

const char* edition_to_string(Edition e) {
    switch (e) {
        case Edition::PROFESSIONAL: return "professional";
        case Edition::ACADEMY:      return "academy";
        case Edition::CASUAL:
        default:                    return "casual";
    }
}

PerfProfile get_profile(Edition edition) {
    PerfProfile p{};

    switch (edition) {
        case Edition::ACADEMY:
            // === Academy: ringan, hemat baterai, low-end friendly ===
            p.cpu_boost_ms        = 0;
            p.sched_boost         = 0;
            p.top_app_cpuset      = 0x0F;   // little cores only
            p.sched_prefer_idle   = true;
            p.sched_uclamp_min    = 0;

            p.gpu_default_governor = 0;      // powersave
            p.gpu_force_max_clock  = false;
            p.gpu_target_fps       = 30;

            p.vm_swappiness        = 100;
            p.vm_dirty_ratio       = 20;
            p.vm_dirty_bg_ratio    = 5;
            p.vm_vfs_cache_pressure = 200;   // aggressive reclaim
            p.zram_enabled         = true;
            p.zram_comp_algo       = 0;      // lz4 (fastest, least CPU)

            p.io_scheduler         = 0;      // noop
            p.io_read_ahead_kb     = 128;
            p.io_nr_requests       = 64;

            p.lmk_minfree_adj     = 1;
            p.lmk_kill_heaviest   = true;
            p.lmk_thrashing_limit = 60;

            p.thermal_throttle_temp = 38;
            p.thermal_adaptive      = false;
            break;

        case Edition::CASUAL:
            // === Casual: balanced midrange, smooth tanpa bloat ===
            p.cpu_boost_ms        = 500;
            p.sched_boost         = 1;       // partial boost
            p.top_app_cpuset      = 0x3F;    // mid + big cores
            p.sched_prefer_idle   = false;
            p.sched_uclamp_min    = 256;

            p.gpu_default_governor = 1;      // balanced
            p.gpu_force_max_clock  = false;
            p.gpu_target_fps       = 60;

            p.vm_swappiness        = 60;
            p.vm_dirty_ratio       = 30;
            p.vm_dirty_bg_ratio    = 10;
            p.vm_vfs_cache_pressure = 100;
            p.zram_enabled         = true;
            p.zram_comp_algo       = 0;      // lz4

            p.io_scheduler         = 2;      // bfq
            p.io_read_ahead_kb     = 512;
            p.io_nr_requests       = 128;

            p.lmk_minfree_adj     = 2;
            p.lmk_kill_heaviest   = true;
            p.lmk_thrashing_limit = 30;

            p.thermal_throttle_temp = 42;
            p.thermal_adaptive      = true;
            break;

        case Edition::PROFESSIONAL:
            // === Professional: max performa, GPU penuh, rata kanan ===
            p.cpu_boost_ms        = 2000;
            p.sched_boost         = 2;       // full boost
            p.top_app_cpuset      = 0xFF;    // all cores
            p.sched_prefer_idle   = false;
            p.sched_uclamp_min    = 512;

            p.gpu_default_governor = 2;      // performance
            p.gpu_force_max_clock  = true;
            p.gpu_target_fps       = 120;

            p.vm_swappiness        = 10;
            p.vm_dirty_ratio       = 40;
            p.vm_dirty_bg_ratio    = 20;
            p.vm_vfs_cache_pressure = 50;    // keep caches longer
            p.zram_enabled         = false;
            p.zram_comp_algo       = 1;      // zstd (better ratio)

            p.io_scheduler         = 3;      // mq-deadline
            p.io_read_ahead_kb     = 2048;
            p.io_nr_requests       = 256;

            p.lmk_minfree_adj     = 3;
            p.lmk_kill_heaviest   = true;
            p.lmk_thrashing_limit = 20;

            p.thermal_throttle_temp = 50;
            p.thermal_adaptive      = true;
            break;
    }

    return p;
}

} // namespace scandium
