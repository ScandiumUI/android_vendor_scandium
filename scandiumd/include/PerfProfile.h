// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

namespace scandium {

enum class Edition {
    ACADEMY,
    CASUAL,
    PROFESSIONAL
};

enum class PerfMode {
    DEFAULT,
    GAMING,
    BATTERY_SAVER,
    SUSTAINED
};

struct CpuProfile {
    int boost_duration_ms;
    int sched_boost;
    int top_app_cpuset;
    int foreground_cpuset;
    int background_cpuset;
    bool sched_prefer_idle;
    int sched_uclamp_min;
    int sched_uclamp_max;
    int sched_uclamp_min_top_app;
    int sched_uclamp_max_top_app;
    int sched_nr_migrate;
    int sched_migration_cost_ns;
    bool energy_aware;
};

struct GpuProfile {
    int governor;
    bool force_max_clock;
    int target_fps;
    int bus_split;
    bool force_bus_on;
    bool force_clk_on;
    bool force_rail_on;
    int idle_timer;
    int adreno_idler_delay;
    bool adreno_idler_active;
    int gpu_throttle_level;
};

struct MemProfile {
    int swappiness;
    int dirty_ratio;
    int dirty_bg_ratio;
    int vfs_cache_pressure;
    bool zram_enabled;
    int zram_comp_algo;
    int zram_max_comp_streams;
    int page_cluster;
    int dirty_expire_cs;
    int dirty_writeback_cs;
    int overcommit_memory;
    int overcommit_ratio;
    int min_free_kbytes;
    int extra_free_kbytes;
    int watermark_scale_factor;
    bool compact_memory;
    int oom_kill_allocating_task;
    int stat_interval;
};

struct IoProfile {
    int scheduler;
    int read_ahead_kb;
    int nr_requests;
    int rq_affinity;
    bool iostats;
    int nomerges;
    bool add_random;
};

struct NetProfile {
    std::string tcp_congestion;
    int tcp_rmem_min;
    int tcp_rmem_default;
    int tcp_rmem_max;
    int tcp_wmem_min;
    int tcp_wmem_default;
    int tcp_wmem_max;
    int somaxconn;
    int netdev_max_backlog;
    bool tcp_fastopen;
    int tcp_fin_timeout;
    int tcp_keepalive_time;
    int tcp_keepalive_intvl;
    int tcp_keepalive_probes;
    bool tcp_tw_reuse;
    bool tcp_sack;
    bool tcp_timestamps;
    bool tcp_window_scaling;
    int tcp_max_syn_backlog;
    int optmem_max;
};

struct SchedProfile {
    int sched_latency_ns;
    int sched_min_granularity_ns;
    int sched_wakeup_granularity_ns;
    int sched_child_runs_first;
    int sched_tunable_scaling;
    int sched_autogroup_enabled;
    int kernel_perf_event_max_sample_rate;
};

struct DisplayProfile {
    int preferred_refresh_rate;
    int touch_boost_duration_ms;
    int sf_phase_offset_ns;
    int vsync_phase_offset_ns;
    bool disable_backpressure;
    bool hw_vsync;
    int gpu_comp_tiling;
    int present_time_offset_ns;
};

struct ThermalProfile {
    int throttle_temp;
    bool adaptive;
    int poll_interval_ms;
    int trip_point_0;
    int trip_point_1;
    int trip_point_2;
    int critical_temp;
};

struct LmkProfile {
    int minfree_adj;
    bool kill_heaviest;
    int thrashing_limit;
    int swap_free_low_pct;
    int psi_complete_stall_ms;
    int psi_partial_stall_ms;
};

struct KernelProfile {
    int printk_level;
    int hung_task_timeout;
    int panic_on_oops;
    int sched_rt_runtime_us;
    int sched_rt_period_us;
    int randomize_va_space;
    int perf_cpu_time_max_pct;
    int entropy_read_wakeup;
    int entropy_write_wakeup;
};

struct FsProfile {
    bool f2fs_gc_enable;
    int f2fs_gc_urgent_sleep_time;
    int f2fs_gc_min_sleep_time;
    int f2fs_gc_max_sleep_time;
    bool f2fs_iostat_enable;
    bool ext4_lazyinit;
    int ext4_commit_interval;
    int inotify_max_user_watches;
    int inotify_max_queued_events;
};

struct IrqProfile {
    bool balance_irqs;
    int rps_cpus_mask;
    int xps_cpus_mask;
    bool rfs_enabled;
    int rfs_flow_entries;
};

struct PerfProfile {
    CpuProfile cpu;
    GpuProfile gpu;
    MemProfile mem;
    IoProfile io;
    NetProfile net;
    SchedProfile sched;
    DisplayProfile display;
    ThermalProfile thermal;
    LmkProfile lmk;
    KernelProfile kernel;
    FsProfile fs;
    IrqProfile irq;
};

struct GameEntry {
    std::string package_name;
    PerfMode mode;
    int priority;
};

Edition edition_from_string(const char* str);
const char* edition_to_string(Edition e);
const char* perfmode_to_string(PerfMode m);
PerfProfile get_profile(Edition edition);
PerfProfile get_gaming_profile(Edition edition);
std::vector<GameEntry> get_game_database();

} // namespace scandium
