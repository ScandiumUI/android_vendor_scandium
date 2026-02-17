// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ScandiumPerfd.h"
#include "CpuTuner.h"
#include "GpuTuner.h"
#include "GpuSpoof.h"
#include "MemoryTuner.h"
#include "IoTuner.h"
#include "ThermalMonitor.h"
#include "NetworkTuner.h"
#include "SchedTuner.h"
#include "DisplayTuner.h"
#include "KernelTuner.h"
#include "FsOptimizer.h"
#include "InterruptBalancer.h"
#include "LmkTuner.h"
#include "GameDetector.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>

namespace scandium {

static constexpr int MAX_EPOLL_EVENTS = 8;
static constexpr int MONITOR_INTERVAL_SEC = 3;
static constexpr int THERMAL_CHECK_SEC = 10;

ScandiumPerfd::ScandiumPerfd()
    : mEdition(detect_edition()),
      mCurrentMode(PerfMode::DEFAULT),
      mBaseProfile(get_profile(mEdition)),
      mActiveProfile(mBaseProfile),
      mRunning(false),
      mGameDetected(false),
      mEpollFd(-1),
      mInotifyFd(-1),
      mProcWatchFd(-1),
      mTimerFd(-1) {
}

ScandiumPerfd::~ScandiumPerfd() {
    cleanup_event_loop();
}

Edition ScandiumPerfd::detect_edition() {
    std::string level = android::base::GetProperty("persist.scandium.perf.level", "balanced");
    if (level == "aggressive") return Edition::PROFESSIONAL;
    if (level == "efficient")  return Edition::ACADEMY;
    return Edition::CASUAL;
}

int ScandiumPerfd::run() {
    log_hardware_info();
    LOG(INFO) << "scandiumd: edition=" << edition_to_string(mEdition)
              << " mode=" << perfmode_to_string(mCurrentMode);

    apply_base_profile();

    bool persistent = android::base::GetBoolProperty(
        "persist.scandium.daemon.persistent", true);

    if (!persistent) {
        LOG(INFO) << "scandiumd: oneshot mode — base profile applied, exiting";
        return 0;
    }

    LOG(INFO) << "scandiumd: persistent mode — starting event loop";
    setup_event_loop();
    run_event_loop();

    return 0;
}

void ScandiumPerfd::request_shutdown() {
    LOG(INFO) << "scandiumd: shutdown requested";
    mRunning.store(false);
}

void ScandiumPerfd::apply_base_profile() {
    LOG(INFO) << "scandiumd: ──── applying base profile ────";
    std::lock_guard<std::mutex> lock(mProfileMutex);

    mActiveProfile = mBaseProfile;
    mCurrentMode = PerfMode::DEFAULT;

    auto start = std::chrono::steady_clock::now();

    CpuTuner::apply(mActiveProfile.cpu);
    GpuTuner::apply(mActiveProfile.gpu);
    GpuSpoof::apply(mEdition);
    MemoryTuner::apply(mActiveProfile.mem);
    IoTuner::apply(mActiveProfile.io);
    ThermalMonitor::apply(mActiveProfile.thermal);
    NetworkTuner::apply(mActiveProfile.net);
    SchedTuner::apply(mActiveProfile.sched);
    DisplayTuner::apply(mActiveProfile.display);
    KernelTuner::apply(mActiveProfile.kernel);
    FsOptimizer::apply(mActiveProfile.fs);
    InterruptBalancer::apply(mActiveProfile.irq);
    LmkTuner::apply(mActiveProfile.lmk);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    log_profile("base", mActiveProfile);
    LOG(INFO) << "scandiumd: ──── base profile applied in " << ms << "ms ────";

    android::base::SetProperty("persist.scandium.daemon.status", "active");
    android::base::SetProperty("persist.scandium.daemon.last_apply",
        std::to_string(std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())));
}

void ScandiumPerfd::apply_gaming_profile(const std::string& package) {
    LOG(INFO) << "scandiumd: ═══ GAME DETECTED: " << package << " ═══";
    std::lock_guard<std::mutex> lock(mProfileMutex);

    mActiveProfile = get_gaming_profile(mEdition);
    mCurrentMode = PerfMode::GAMING;
    mCurrentGame = package;
    mGameDetected.store(true);

    auto start = std::chrono::steady_clock::now();

    MemoryTuner::drop_caches();
    MemoryTuner::compact_memory();

    CpuTuner::apply(mActiveProfile.cpu);
    GpuTuner::apply(mActiveProfile.gpu);
    MemoryTuner::apply(mActiveProfile.mem);
    IoTuner::apply(mActiveProfile.io);
    SchedTuner::apply(mActiveProfile.sched);
    DisplayTuner::apply(mActiveProfile.display);
    NetworkTuner::apply(mActiveProfile.net);

    CpuTuner::apply_boost(mActiveProfile.cpu.boost_duration_ms);
    DisplayTuner::apply_touch_boost(mActiveProfile.display.touch_boost_duration_ms);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    log_profile("gaming", mActiveProfile);
    LOG(INFO) << "scandiumd: ═══ gaming profile applied in " << ms << "ms ═══";

    android::base::SetProperty("persist.scandium.daemon.mode", "gaming");
    android::base::SetProperty("persist.scandium.daemon.game", package);
}

void ScandiumPerfd::revert_to_base() {
    if (!mGameDetected.load()) return;

    LOG(INFO) << "scandiumd: ──── game exited, reverting to base ────";
    mGameDetected.store(false);
    mCurrentGame.clear();

    apply_base_profile();

    android::base::SetProperty("persist.scandium.daemon.mode", "default");
    android::base::SetProperty("persist.scandium.daemon.game", "");
}

void ScandiumPerfd::switch_mode(PerfMode mode) {
    if (mode == mCurrentMode) return;

    LOG(INFO) << "scandiumd: switching mode to " << perfmode_to_string(mode);
    mCurrentMode = mode;

    switch (mode) {
        case PerfMode::DEFAULT:
            apply_base_profile();
            break;
        case PerfMode::GAMING:
            break;
        case PerfMode::BATTERY_SAVER: {
            std::lock_guard<std::mutex> lock(mProfileMutex);
            PerfProfile saver = get_profile(Edition::ACADEMY);
            mActiveProfile = saver;
            CpuTuner::apply(saver.cpu);
            GpuTuner::apply(saver.gpu);
            MemoryTuner::apply(saver.mem);
            DisplayTuner::apply(saver.display);
            break;
        }
        case PerfMode::SUSTAINED: {
            std::lock_guard<std::mutex> lock(mProfileMutex);
            PerfProfile sustained = get_profile(Edition::CASUAL);
            sustained.thermal.throttle_temp -= 5;
            sustained.cpu.sched_boost = 1;
            mActiveProfile = sustained;
            CpuTuner::apply(sustained.cpu);
            ThermalMonitor::apply(sustained.thermal);
            break;
        }
    }

    android::base::SetProperty("persist.scandium.daemon.mode",
        perfmode_to_string(mode));
}

void ScandiumPerfd::setup_event_loop() {
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (mEpollFd < 0) {
        LOG(ERROR) << "scandiumd: epoll_create1 failed: " << strerror(errno);
        return;
    }

    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (mTimerFd >= 0) {
        struct itimerspec ts;
        ts.it_value.tv_sec = MONITOR_INTERVAL_SEC;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = MONITOR_INTERVAL_SEC;
        ts.it_interval.tv_nsec = 0;
        timerfd_settime(mTimerFd, 0, &ts, nullptr);

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = mTimerFd;
        epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mTimerFd, &ev);
    }

    setup_proc_monitor();
}

void ScandiumPerfd::setup_proc_monitor() {
    mInotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (mInotifyFd < 0) {
        LOG(ERROR) << "scandiumd: inotify_init1 failed: " << strerror(errno);
        return;
    }

    mProcWatchFd = inotify_add_watch(mInotifyFd, "/proc",
        IN_CREATE | IN_DELETE);

    if (mProcWatchFd < 0) {
        LOG(WARNING) << "scandiumd: cannot watch /proc: " << strerror(errno);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = mInotifyFd;
    epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mInotifyFd, &ev);

    LOG(INFO) << "scandiumd: inotify + epoll event loop ready";
}

void ScandiumPerfd::run_event_loop() {
    mRunning.store(true);
    struct epoll_event events[MAX_EPOLL_EVENTS];

    GameDetector detector;
    detector.load_game_database();

    LOG(INFO) << "scandiumd: event loop started, monitoring...";

    while (mRunning.load()) {
        int nfds = epoll_wait(mEpollFd, events, MAX_EPOLL_EVENTS, 5000);

        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG(ERROR) << "scandiumd: epoll_wait error: " << strerror(errno);
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == mInotifyFd) {
                handle_proc_event();
            } else if (events[i].data.fd == mTimerFd) {
                handle_timer_event();
            }
        }

        std::string fg_pkg = detector.detect_foreground_package();
        if (!fg_pkg.empty() && fg_pkg != mCurrentGame) {
            if (detector.is_game(fg_pkg)) {
                apply_gaming_profile(fg_pkg);
            } else if (mGameDetected.load()) {
                revert_to_base();
            }
        }

        std::string new_level = android::base::GetProperty(
            "persist.scandium.perf.level", "balanced");
        Edition new_edition;
        if (new_level == "aggressive") new_edition = Edition::PROFESSIONAL;
        else if (new_level == "efficient") new_edition = Edition::ACADEMY;
        else new_edition = Edition::CASUAL;

        if (new_edition != mEdition) {
            LOG(INFO) << "scandiumd: edition changed to "
                      << edition_to_string(new_edition);
            mEdition = new_edition;
            mBaseProfile = get_profile(mEdition);
            if (!mGameDetected.load()) {
                apply_base_profile();
            }
        }
    }

    LOG(INFO) << "scandiumd: event loop exited";
}

void ScandiumPerfd::handle_proc_event() {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(mInotifyFd, buf, sizeof(buf));
    (void)len;
}

void ScandiumPerfd::handle_timer_event() {
    uint64_t expirations;
    read(mTimerFd, &expirations, sizeof(expirations));

    int cpu_temp = ThermalMonitor::read_cpu_temp();
    if (cpu_temp >= 0) {
        int threshold = mActiveProfile.thermal.throttle_temp;

        if (cpu_temp >= threshold && mCurrentMode == PerfMode::GAMING) {
            LOG(WARNING) << "scandiumd: thermal throttle! temp=" << cpu_temp
                         << "°C >= " << threshold << "°C, entering sustained mode";
            switch_mode(PerfMode::SUSTAINED);
        } else if (cpu_temp < (threshold - 10) && mCurrentMode == PerfMode::SUSTAINED) {
            LOG(INFO) << "scandiumd: thermal recovered, temp=" << cpu_temp << "°C";
            if (mGameDetected.load()) {
                switch_mode(PerfMode::GAMING);
                apply_gaming_profile(mCurrentGame);
            } else {
                switch_mode(PerfMode::DEFAULT);
            }
        }
    }

    long avail_ram = MemoryTuner::get_available_ram_kb();
    if (avail_ram > 0 && avail_ram < 200 * 1024) {
        LOG(WARNING) << "scandiumd: low memory! available=" << (avail_ram / 1024) << "MB";
        MemoryTuner::drop_caches();
    }
}

void ScandiumPerfd::cleanup_event_loop() {
    if (mProcWatchFd >= 0 && mInotifyFd >= 0) {
        inotify_rm_watch(mInotifyFd, mProcWatchFd);
        mProcWatchFd = -1;
    }
    if (mInotifyFd >= 0) { close(mInotifyFd); mInotifyFd = -1; }
    if (mTimerFd >= 0) { close(mTimerFd); mTimerFd = -1; }
    if (mEpollFd >= 0) { close(mEpollFd); mEpollFd = -1; }
}

std::string ScandiumPerfd::detect_foreground_app() {
    std::string dumpsys;
    DIR* dir = opendir("/proc");
    if (!dir) return "";

    pid_t best_pid = -1;
    std::string best_pkg;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        if (!isdigit(entry->d_name[0])) continue;

        pid_t pid = atoi(entry->d_name);
        std::string oom_path = android::base::StringPrintf("/proc/%d/oom_score_adj", pid);
        std::string oom_str;
        if (!android::base::ReadFileToString(oom_path, &oom_str)) continue;

        int oom = atoi(oom_str.c_str());
        if (oom == 0) {
            std::string cmdline_path = android::base::StringPrintf("/proc/%d/cmdline", pid);
            std::string cmdline;
            if (android::base::ReadFileToString(cmdline_path, &cmdline)) {
                size_t null_pos = cmdline.find('\0');
                if (null_pos != std::string::npos) cmdline.resize(null_pos);
                if (!cmdline.empty() && cmdline.find('.') != std::string::npos) {
                    best_pid = pid;
                    best_pkg = cmdline;
                }
            }
        }
    }
    closedir(dir);

    return best_pkg;
}

bool ScandiumPerfd::is_game_package(const std::string& package) {
    auto db = get_game_database();
    for (const auto& entry : db) {
        if (package.find(entry.package_name) != std::string::npos)
            return true;
    }
    return false;
}

void ScandiumPerfd::log_profile(const char* tag, const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: [" << tag << "] CPU: boost=" << profile.cpu.boost_duration_ms
              << "ms sched_boost=" << profile.cpu.sched_boost
              << " uclamp_min=" << profile.cpu.sched_uclamp_min
              << " cpuset=0x" << std::hex << profile.cpu.top_app_cpuset << std::dec
              << " eas=" << profile.cpu.energy_aware;
    LOG(INFO) << "scandiumd: [" << tag << "] GPU: gov=" << profile.gpu.governor
              << " force_max=" << profile.gpu.force_max_clock
              << " fps=" << profile.gpu.target_fps
              << " idler=" << profile.gpu.adreno_idler_active;
    LOG(INFO) << "scandiumd: [" << tag << "] MEM: swap=" << profile.mem.swappiness
              << " dirty=" << profile.mem.dirty_ratio
              << " cache=" << profile.mem.vfs_cache_pressure
              << " zram=" << profile.mem.zram_enabled
              << " min_free=" << profile.mem.min_free_kbytes;
    LOG(INFO) << "scandiumd: [" << tag << "] IO: sched=" << profile.io.scheduler
              << " readahead=" << profile.io.read_ahead_kb
              << " nr_req=" << profile.io.nr_requests;
    LOG(INFO) << "scandiumd: [" << tag << "] NET: congestion=" << profile.net.tcp_congestion
              << " rmem_max=" << profile.net.tcp_rmem_max
              << " wmem_max=" << profile.net.tcp_wmem_max;
    LOG(INFO) << "scandiumd: [" << tag << "] SCHED: latency=" << profile.sched.sched_latency_ns
              << " granularity=" << profile.sched.sched_min_granularity_ns;
    LOG(INFO) << "scandiumd: [" << tag << "] DISPLAY: refresh=" << profile.display.preferred_refresh_rate
              << "Hz touch_boost=" << profile.display.touch_boost_duration_ms << "ms";
    LOG(INFO) << "scandiumd: [" << tag << "] THERMAL: throttle=" << profile.thermal.throttle_temp
              << "°C adaptive=" << profile.thermal.adaptive;
    LOG(INFO) << "scandiumd: [" << tag << "] LMK: adj=" << profile.lmk.minfree_adj
              << " thrash=" << profile.lmk.thrashing_limit;
    LOG(INFO) << "scandiumd: [" << tag << "] IRQ: balance=" << profile.irq.balance_irqs
              << " rps=0x" << std::hex << profile.irq.rps_cpus_mask << std::dec;
}

void ScandiumPerfd::log_hardware_info() {
    LOG(INFO) << "scandiumd: ── Hardware Detection ──";
    LOG(INFO) << "scandiumd: CPU cores: " << get_cpu_count();
    LOG(INFO) << "scandiumd: RAM: " << get_total_ram_mb() << " MB";
    LOG(INFO) << "scandiumd: GPU: " << get_gpu_model();
    LOG(INFO) << "scandiumd: SoC: " << get_soc_model();
    LOG(INFO) << "scandiumd: ────────────────────────";
}

int ScandiumPerfd::get_cpu_count() {
    return sysconf(_SC_NPROCESSORS_CONF);
}

long ScandiumPerfd::get_total_ram_mb() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return (si.totalram * si.mem_unit) / (1024 * 1024);
    }
    return 0;
}

std::string ScandiumPerfd::get_gpu_model() {
    struct stat st;
    if (stat("/sys/class/kgsl/kgsl-3d0", &st) == 0) {
        std::string gpu_model;
        if (android::base::ReadFileToString(
                "/sys/class/kgsl/kgsl-3d0/gpu_model", &gpu_model)) {
            gpu_model.erase(gpu_model.find_last_not_of("\n\r ") + 1);
            return "Adreno " + gpu_model;
        }
        return "Adreno (unknown model)";
    }

    const char* mali_paths[] = {
        "/sys/devices/platform/mali.0",
        "/sys/devices/platform/gpu",
        nullptr
    };
    for (int i = 0; mali_paths[i]; i++) {
        if (stat(mali_paths[i], &st) == 0) return "Mali GPU";
    }

    if (stat("/sys/devices/platform/pvrsrvkm", &st) == 0)
        return "PowerVR GPU";

    return "Unknown GPU";
}

std::string ScandiumPerfd::get_soc_model() {
    std::string soc = android::base::GetProperty("ro.soc.model", "");
    if (!soc.empty()) return soc;

    soc = android::base::GetProperty("ro.board.platform", "");
    if (!soc.empty()) return soc;

    std::string cpuinfo;
    if (android::base::ReadFileToString("/proc/cpuinfo", &cpuinfo)) {
        std::istringstream iss(cpuinfo);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Hardware") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string hw = line.substr(pos + 1);
                    hw.erase(0, hw.find_first_not_of(" \t"));
                    return hw;
                }
            }
        }
    }

    return "Unknown SoC";
}

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

const char* perfmode_to_string(PerfMode m) {
    switch (m) {
        case PerfMode::GAMING:        return "gaming";
        case PerfMode::BATTERY_SAVER: return "battery_saver";
        case PerfMode::SUSTAINED:     return "sustained";
        case PerfMode::DEFAULT:
        default:                      return "default";
    }
}

PerfProfile get_profile(Edition edition) {
    PerfProfile p{};

    switch (edition) {
        case Edition::ACADEMY:
            // CPU
            p.cpu.boost_duration_ms = 0;
            p.cpu.sched_boost = 0;
            p.cpu.top_app_cpuset = 0x0F;
            p.cpu.foreground_cpuset = 0x0F;
            p.cpu.background_cpuset = 0x03;
            p.cpu.sched_prefer_idle = true;
            p.cpu.sched_uclamp_min = 0;
            p.cpu.sched_uclamp_max = 768;
            p.cpu.sched_uclamp_min_top_app = 0;
            p.cpu.sched_uclamp_max_top_app = 768;
            p.cpu.sched_nr_migrate = 4;
            p.cpu.sched_migration_cost_ns = 5000000;
            p.cpu.energy_aware = true;

            // GPU
            p.gpu.governor = 0;
            p.gpu.force_max_clock = false;
            p.gpu.target_fps = 30;
            p.gpu.bus_split = 1;
            p.gpu.force_bus_on = false;
            p.gpu.force_clk_on = false;
            p.gpu.force_rail_on = false;
            p.gpu.idle_timer = 64;
            p.gpu.adreno_idler_delay = 15;
            p.gpu.adreno_idler_active = true;
            p.gpu.gpu_throttle_level = 2;

            // Memory
            p.mem.swappiness = 100;
            p.mem.dirty_ratio = 20;
            p.mem.dirty_bg_ratio = 5;
            p.mem.vfs_cache_pressure = 200;
            p.mem.zram_enabled = true;
            p.mem.zram_comp_algo = 0;
            p.mem.zram_max_comp_streams = 2;
            p.mem.page_cluster = 3;
            p.mem.dirty_expire_cs = 1000;
            p.mem.dirty_writeback_cs = 500;
            p.mem.overcommit_memory = 0;
            p.mem.overcommit_ratio = 50;
            p.mem.min_free_kbytes = 8192;
            p.mem.extra_free_kbytes = 4096;
            p.mem.watermark_scale_factor = 30;
            p.mem.compact_memory = false;
            p.mem.oom_kill_allocating_task = 0;
            p.mem.stat_interval = 10;

            // I/O
            p.io.scheduler = 0;
            p.io.read_ahead_kb = 128;
            p.io.nr_requests = 64;
            p.io.rq_affinity = 1;
            p.io.iostats = false;
            p.io.nomerges = 0;
            p.io.add_random = false;

            // Network
            p.net.tcp_congestion = "cubic";
            p.net.tcp_rmem_min = 4096;
            p.net.tcp_rmem_default = 131072;
            p.net.tcp_rmem_max = 1048576;
            p.net.tcp_wmem_min = 4096;
            p.net.tcp_wmem_default = 16384;
            p.net.tcp_wmem_max = 1048576;
            p.net.somaxconn = 128;
            p.net.netdev_max_backlog = 1000;
            p.net.tcp_fastopen = false;
            p.net.tcp_fin_timeout = 60;
            p.net.tcp_keepalive_time = 7200;
            p.net.tcp_keepalive_intvl = 75;
            p.net.tcp_keepalive_probes = 9;
            p.net.tcp_tw_reuse = false;
            p.net.tcp_sack = true;
            p.net.tcp_timestamps = true;
            p.net.tcp_window_scaling = true;
            p.net.tcp_max_syn_backlog = 256;
            p.net.optmem_max = 20480;

            // Scheduler
            p.sched.sched_latency_ns = 10000000;
            p.sched.sched_min_granularity_ns = 2250000;
            p.sched.sched_wakeup_granularity_ns = 3000000;
            p.sched.sched_child_runs_first = 0;
            p.sched.sched_tunable_scaling = 1;
            p.sched.sched_autogroup_enabled = 1;
            p.sched.kernel_perf_event_max_sample_rate = 50000;

            // Display
            p.display.preferred_refresh_rate = 60;
            p.display.touch_boost_duration_ms = 0;
            p.display.sf_phase_offset_ns = 6000000;
            p.display.vsync_phase_offset_ns = 2000000;
            p.display.disable_backpressure = false;
            p.display.hw_vsync = true;
            p.display.gpu_comp_tiling = 0;
            p.display.present_time_offset_ns = 0;

            // Thermal
            p.thermal.throttle_temp = 38;
            p.thermal.adaptive = false;
            p.thermal.poll_interval_ms = 5000;
            p.thermal.trip_point_0 = 38000;
            p.thermal.trip_point_1 = 45000;
            p.thermal.trip_point_2 = 55000;
            p.thermal.critical_temp = 65000;

            // LMK
            p.lmk.minfree_adj = 1;
            p.lmk.kill_heaviest = true;
            p.lmk.thrashing_limit = 60;
            p.lmk.swap_free_low_pct = 20;
            p.lmk.psi_complete_stall_ms = 300;
            p.lmk.psi_partial_stall_ms = 200;

            // Kernel
            p.kernel.printk_level = 4;
            p.kernel.hung_task_timeout = 120;
            p.kernel.panic_on_oops = 0;
            p.kernel.sched_rt_runtime_us = 950000;
            p.kernel.sched_rt_period_us = 1000000;
            p.kernel.randomize_va_space = 2;
            p.kernel.perf_cpu_time_max_pct = 5;
            p.kernel.entropy_read_wakeup = 64;
            p.kernel.entropy_write_wakeup = 128;

            // Filesystem
            p.fs.f2fs_gc_enable = true;
            p.fs.f2fs_gc_urgent_sleep_time = 500;
            p.fs.f2fs_gc_min_sleep_time = 30000;
            p.fs.f2fs_gc_max_sleep_time = 60000;
            p.fs.f2fs_iostat_enable = false;
            p.fs.ext4_lazyinit = true;
            p.fs.ext4_commit_interval = 30;
            p.fs.inotify_max_user_watches = 32768;
            p.fs.inotify_max_queued_events = 16384;

            // IRQ
            p.irq.balance_irqs = false;
            p.irq.rps_cpus_mask = 0x0F;
            p.irq.xps_cpus_mask = 0x0F;
            p.irq.rfs_enabled = false;
            p.irq.rfs_flow_entries = 0;
            break;

        case Edition::CASUAL:
            // CPU
            p.cpu.boost_duration_ms = 500;
            p.cpu.sched_boost = 1;
            p.cpu.top_app_cpuset = 0x3F;
            p.cpu.foreground_cpuset = 0x3F;
            p.cpu.background_cpuset = 0x0F;
            p.cpu.sched_prefer_idle = false;
            p.cpu.sched_uclamp_min = 256;
            p.cpu.sched_uclamp_max = 1024;
            p.cpu.sched_uclamp_min_top_app = 256;
            p.cpu.sched_uclamp_max_top_app = 1024;
            p.cpu.sched_nr_migrate = 8;
            p.cpu.sched_migration_cost_ns = 500000;
            p.cpu.energy_aware = true;

            // GPU
            p.gpu.governor = 1;
            p.gpu.force_max_clock = false;
            p.gpu.target_fps = 60;
            p.gpu.bus_split = 0;
            p.gpu.force_bus_on = false;
            p.gpu.force_clk_on = false;
            p.gpu.force_rail_on = false;
            p.gpu.idle_timer = 80;
            p.gpu.adreno_idler_delay = 10;
            p.gpu.adreno_idler_active = true;
            p.gpu.gpu_throttle_level = 1;

            // Memory
            p.mem.swappiness = 60;
            p.mem.dirty_ratio = 30;
            p.mem.dirty_bg_ratio = 10;
            p.mem.vfs_cache_pressure = 100;
            p.mem.zram_enabled = true;
            p.mem.zram_comp_algo = 0;
            p.mem.zram_max_comp_streams = 4;
            p.mem.page_cluster = 0;
            p.mem.dirty_expire_cs = 3000;
            p.mem.dirty_writeback_cs = 1500;
            p.mem.overcommit_memory = 0;
            p.mem.overcommit_ratio = 50;
            p.mem.min_free_kbytes = 16384;
            p.mem.extra_free_kbytes = 8192;
            p.mem.watermark_scale_factor = 20;
            p.mem.compact_memory = false;
            p.mem.oom_kill_allocating_task = 0;
            p.mem.stat_interval = 5;

            // I/O
            p.io.scheduler = 2;
            p.io.read_ahead_kb = 512;
            p.io.nr_requests = 128;
            p.io.rq_affinity = 1;
            p.io.iostats = false;
            p.io.nomerges = 0;
            p.io.add_random = false;

            // Network
            p.net.tcp_congestion = "bbr";
            p.net.tcp_rmem_min = 4096;
            p.net.tcp_rmem_default = 262144;
            p.net.tcp_rmem_max = 4194304;
            p.net.tcp_wmem_min = 4096;
            p.net.tcp_wmem_default = 65536;
            p.net.tcp_wmem_max = 4194304;
            p.net.somaxconn = 256;
            p.net.netdev_max_backlog = 2000;
            p.net.tcp_fastopen = true;
            p.net.tcp_fin_timeout = 30;
            p.net.tcp_keepalive_time = 1800;
            p.net.tcp_keepalive_intvl = 30;
            p.net.tcp_keepalive_probes = 5;
            p.net.tcp_tw_reuse = true;
            p.net.tcp_sack = true;
            p.net.tcp_timestamps = true;
            p.net.tcp_window_scaling = true;
            p.net.tcp_max_syn_backlog = 512;
            p.net.optmem_max = 65536;

            // Scheduler
            p.sched.sched_latency_ns = 6000000;
            p.sched.sched_min_granularity_ns = 750000;
            p.sched.sched_wakeup_granularity_ns = 1000000;
            p.sched.sched_child_runs_first = 0;
            p.sched.sched_tunable_scaling = 0;
            p.sched.sched_autogroup_enabled = 1;
            p.sched.kernel_perf_event_max_sample_rate = 100000;

            // Display
            p.display.preferred_refresh_rate = 90;
            p.display.touch_boost_duration_ms = 500;
            p.display.sf_phase_offset_ns = 4000000;
            p.display.vsync_phase_offset_ns = 1000000;
            p.display.disable_backpressure = true;
            p.display.hw_vsync = true;
            p.display.gpu_comp_tiling = 1;
            p.display.present_time_offset_ns = 0;

            // Thermal
            p.thermal.throttle_temp = 42;
            p.thermal.adaptive = true;
            p.thermal.poll_interval_ms = 3000;
            p.thermal.trip_point_0 = 42000;
            p.thermal.trip_point_1 = 50000;
            p.thermal.trip_point_2 = 60000;
            p.thermal.critical_temp = 70000;

            // LMK
            p.lmk.minfree_adj = 2;
            p.lmk.kill_heaviest = true;
            p.lmk.thrashing_limit = 30;
            p.lmk.swap_free_low_pct = 10;
            p.lmk.psi_complete_stall_ms = 200;
            p.lmk.psi_partial_stall_ms = 100;

            // Kernel
            p.kernel.printk_level = 3;
            p.kernel.hung_task_timeout = 0;
            p.kernel.panic_on_oops = 0;
            p.kernel.sched_rt_runtime_us = 950000;
            p.kernel.sched_rt_period_us = 1000000;
            p.kernel.randomize_va_space = 2;
            p.kernel.perf_cpu_time_max_pct = 10;
            p.kernel.entropy_read_wakeup = 128;
            p.kernel.entropy_write_wakeup = 256;

            // Filesystem
            p.fs.f2fs_gc_enable = true;
            p.fs.f2fs_gc_urgent_sleep_time = 500;
            p.fs.f2fs_gc_min_sleep_time = 30000;
            p.fs.f2fs_gc_max_sleep_time = 60000;
            p.fs.f2fs_iostat_enable = false;
            p.fs.ext4_lazyinit = true;
            p.fs.ext4_commit_interval = 15;
            p.fs.inotify_max_user_watches = 65536;
            p.fs.inotify_max_queued_events = 32768;

            // IRQ
            p.irq.balance_irqs = true;
            p.irq.rps_cpus_mask = 0x3F;
            p.irq.xps_cpus_mask = 0x3F;
            p.irq.rfs_enabled = true;
            p.irq.rfs_flow_entries = 32768;
            break;

        case Edition::PROFESSIONAL:
            // CPU
            p.cpu.boost_duration_ms = 2000;
            p.cpu.sched_boost = 2;
            p.cpu.top_app_cpuset = 0xFF;
            p.cpu.foreground_cpuset = 0xFF;
            p.cpu.background_cpuset = 0x0F;
            p.cpu.sched_prefer_idle = false;
            p.cpu.sched_uclamp_min = 512;
            p.cpu.sched_uclamp_max = 1024;
            p.cpu.sched_uclamp_min_top_app = 614;
            p.cpu.sched_uclamp_max_top_app = 1024;
            p.cpu.sched_nr_migrate = 16;
            p.cpu.sched_migration_cost_ns = 100000;
            p.cpu.energy_aware = false;

            p.gpu.governor = 2;
            p.gpu.force_max_clock = true;
            p.gpu.target_fps = 120;
            p.gpu.bus_split = 0;
            p.gpu.force_bus_on = true;
            p.gpu.force_clk_on = true;
            p.gpu.force_rail_on = true;
            p.gpu.idle_timer = 10000;
            p.gpu.adreno_idler_delay = 0;
            p.gpu.adreno_idler_active = false;
            p.gpu.gpu_throttle_level = 0;

            p.mem.swappiness = 10;
            p.mem.dirty_ratio = 40;
            p.mem.dirty_bg_ratio = 20;
            p.mem.vfs_cache_pressure = 50;
            p.mem.zram_enabled = false;
            p.mem.zram_comp_algo = 1;
            p.mem.zram_max_comp_streams = 8;
            p.mem.page_cluster = 0;
            p.mem.dirty_expire_cs = 3000;
            p.mem.dirty_writeback_cs = 3000;
            p.mem.overcommit_memory = 1;
            p.mem.overcommit_ratio = 100;
            p.mem.min_free_kbytes = 32768;
            p.mem.extra_free_kbytes = 24576;
            p.mem.watermark_scale_factor = 10;
            p.mem.compact_memory = true;
            p.mem.oom_kill_allocating_task = 0;
            p.mem.stat_interval = 1;

            p.io.scheduler = 3;
            p.io.read_ahead_kb = 2048;
            p.io.nr_requests = 256;
            p.io.rq_affinity = 2;
            p.io.iostats = false;
            p.io.nomerges = 2;
            p.io.add_random = false;

            p.net.tcp_congestion = "bbr";
            p.net.tcp_rmem_min = 8192;
            p.net.tcp_rmem_default = 524288;
            p.net.tcp_rmem_max = 16777216;
            p.net.tcp_wmem_min = 8192;
            p.net.tcp_wmem_default = 262144;
            p.net.tcp_wmem_max = 16777216;
            p.net.somaxconn = 4096;
            p.net.netdev_max_backlog = 16384;
            p.net.tcp_fastopen = true;
            p.net.tcp_fin_timeout = 15;
            p.net.tcp_keepalive_time = 600;
            p.net.tcp_keepalive_intvl = 10;
            p.net.tcp_keepalive_probes = 3;
            p.net.tcp_tw_reuse = true;
            p.net.tcp_sack = true;
            p.net.tcp_timestamps = false;
            p.net.tcp_window_scaling = true;
            p.net.tcp_max_syn_backlog = 4096;
            p.net.optmem_max = 524288;

            p.sched.sched_latency_ns = 4000000;
            p.sched.sched_min_granularity_ns = 500000;
            p.sched.sched_wakeup_granularity_ns = 500000;
            p.sched.sched_child_runs_first = 1;
            p.sched.sched_tunable_scaling = 0;
            p.sched.sched_autogroup_enabled = 0;
            p.sched.kernel_perf_event_max_sample_rate = 500000;

            p.display.preferred_refresh_rate = 120;
            p.display.touch_boost_duration_ms = 2000;
            p.display.sf_phase_offset_ns = 2000000;
            p.display.vsync_phase_offset_ns = 500000;
            p.display.disable_backpressure = true;
            p.display.hw_vsync = true;
            p.display.gpu_comp_tiling = 1;
            p.display.present_time_offset_ns = 0;

            p.thermal.throttle_temp = 50;
            p.thermal.adaptive = true;
            p.thermal.poll_interval_ms = 1000;
            p.thermal.trip_point_0 = 50000;
            p.thermal.trip_point_1 = 60000;
            p.thermal.trip_point_2 = 70000;
            p.thermal.critical_temp = 80000;

            p.lmk.minfree_adj = 3;
            p.lmk.kill_heaviest = true;
            p.lmk.thrashing_limit = 20;
            p.lmk.swap_free_low_pct = 5;
            p.lmk.psi_complete_stall_ms = 150;
            p.lmk.psi_partial_stall_ms = 70;

            p.kernel.printk_level = 0;
            p.kernel.hung_task_timeout = 0;
            p.kernel.panic_on_oops = 0;
            p.kernel.sched_rt_runtime_us = 980000;
            p.kernel.sched_rt_period_us = 1000000;
            p.kernel.randomize_va_space = 2;
            p.kernel.perf_cpu_time_max_pct = 25;
            p.kernel.entropy_read_wakeup = 256;
            p.kernel.entropy_write_wakeup = 512;

            p.fs.f2fs_gc_enable = false;
            p.fs.f2fs_gc_urgent_sleep_time = 500;
            p.fs.f2fs_gc_min_sleep_time = 60000;
            p.fs.f2fs_gc_max_sleep_time = 120000;
            p.fs.f2fs_iostat_enable = false;
            p.fs.ext4_lazyinit = true;
            p.fs.ext4_commit_interval = 5;
            p.fs.inotify_max_user_watches = 131072;
            p.fs.inotify_max_queued_events = 65536;

            p.irq.balance_irqs = true;
            p.irq.rps_cpus_mask = 0xF0;
            p.irq.xps_cpus_mask = 0xF0;
            p.irq.rfs_enabled = true;
            p.irq.rfs_flow_entries = 65536;
            break;
    }

    return p;
}

PerfProfile get_gaming_profile(Edition edition) {
    PerfProfile p = get_profile(edition);
    p.cpu.boost_duration_ms = 5000;
    p.cpu.sched_boost = 2;
    p.cpu.top_app_cpuset = 0xFF;
    p.cpu.foreground_cpuset = 0xFF;
    p.cpu.sched_uclamp_min_top_app = 768;
    p.cpu.sched_uclamp_max_top_app = 1024;
    p.cpu.energy_aware = false;

    p.gpu.governor = 2;
    p.gpu.force_max_clock = true;
    p.gpu.force_bus_on = true;
    p.gpu.force_clk_on = true;
    p.gpu.force_rail_on = true;
    p.gpu.adreno_idler_active = false;
    p.gpu.idle_timer = 10000;

    p.mem.compact_memory = true;
    p.mem.vfs_cache_pressure = 50;

    p.io.scheduler = 3;
    p.io.read_ahead_kb = 2048;
    p.io.rq_affinity = 2;

    p.sched.sched_latency_ns = 4000000;
    p.sched.sched_min_granularity_ns = 500000;
    p.sched.sched_wakeup_granularity_ns = 500000;
    p.sched.sched_autogroup_enabled = 0;

    switch (edition) {
        case Edition::ACADEMY:
            p.display.preferred_refresh_rate = 60;
            p.gpu.target_fps = 60;
            break;
        case Edition::CASUAL:
            p.display.preferred_refresh_rate = 90;
            p.gpu.target_fps = 90;
            break;
        case Edition::PROFESSIONAL:
            p.display.preferred_refresh_rate = 120;
            p.gpu.target_fps = 144;
            p.display.touch_boost_duration_ms = 5000;
            break;
    }

    return p;
}

std::vector<GameEntry> get_game_database() {
    return {
        // Battle Royale
        {"com.tencent.ig",              PerfMode::GAMING, 10},
        {"com.pubg.imobile",            PerfMode::GAMING, 10},
        {"com.pubg.krmobile",           PerfMode::GAMING, 10},
        {"com.rekoo.pubgm",             PerfMode::GAMING, 10},
        {"com.tencent.tmgp.pubgmhd",    PerfMode::GAMING, 10},
        {"com.garena.game.kgid",        PerfMode::GAMING, 9},
        {"com.dts.freefiremax",         PerfMode::GAMING, 9},
        {"com.dts.freefireth",          PerfMode::GAMING, 9},
        {"com.epicgames.fortnite",      PerfMode::GAMING, 10},

        // MOBA
        {"com.mobile.legends",          PerfMode::GAMING, 9},
        {"com.mobilelegends.hwag",      PerfMode::GAMING, 9},
        {"com.tencent.lolm",            PerfMode::GAMING, 9},
        {"com.riotgames.league.wildrift", PerfMode::GAMING, 9},
        {"com.garena.game.lol",         PerfMode::GAMING, 9},
        {"com.levelinfinite.hotta.gp",  PerfMode::GAMING, 8},

        // FPS
        {"com.activision.callofduty.shooter", PerfMode::GAMING, 10},
        {"com.tencent.tmgp.cod",        PerfMode::GAMING, 10},
        {"com.ea.gp.apexlegendsmobile", PerfMode::GAMING, 9},
        {"com.criticalforceentertainment.criticalops", PerfMode::GAMING, 8},
        {"com.innersloth.spacemafia",   PerfMode::GAMING, 5},

        // Racing
        {"com.gameloft.android.ANMP.GloftA9HM", PerfMode::GAMING, 9},
        {"com.ea.games.nfs14_row",      PerfMode::GAMING, 8},
        {"com.naturalmotion.customstreetracer2", PerfMode::GAMING, 8},

        // RPG / Open World
        {"com.miHoYo.GenshinImpact",    PerfMode::GAMING, 10},
        {"com.HoYoverse.hkrpgoversea",  PerfMode::GAMING, 10},
        {"com.miHoYo.bh3oversea",       PerfMode::GAMING, 9},
        {"com.kurogame.wutheringwaves.global", PerfMode::GAMING, 10},
        {"com.netease.lztgglobal",      PerfMode::GAMING, 8},
        {"com.tencent.KiHan",           PerfMode::GAMING, 8},

        // Strategy
        {"com.supercell.clashofclans",  PerfMode::GAMING, 5},
        {"com.supercell.clashroyale",   PerfMode::GAMING, 6},
        {"com.supercell.brawlstars",    PerfMode::GAMING, 7},

        // Minecraft
        {"com.mojang.minecraftpe",      PerfMode::GAMING, 7},

        // Emulators
        {"com.dolphinemu.dolphinemu",   PerfMode::GAMING, 10},
        {"org.ppsspp.ppsspp",           PerfMode::GAMING, 9},
        {"org.ppsspp.ppssppgold",       PerfMode::GAMING, 9},
        {"com.retroarch",              PerfMode::GAMING, 8},
        {"org.citra.citra_emu",         PerfMode::GAMING, 10},
        {"org.yuzu.yuzu_emu",           PerfMode::GAMING, 10},
        {"skyline.emu",                 PerfMode::GAMING, 10},
        {"com.switchemu.strato",        PerfMode::GAMING, 10},

        // Benchmarks
        {"com.futuremark.dmandroid.application", PerfMode::GAMING, 10},
        {"com.antutu.ABenchMark",       PerfMode::GAMING, 10},
        {"com.primatelabs.geekbench6",  PerfMode::GAMING, 10},
        {"com.kishonti.gfxbench.gl.v50000", PerfMode::GAMING, 10},
    };
}

} // namespace scandium
