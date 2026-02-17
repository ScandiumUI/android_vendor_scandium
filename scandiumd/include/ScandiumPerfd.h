// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace scandium {

class ScandiumPerfd {
public:
    ScandiumPerfd();
    ~ScandiumPerfd();

    int run();
    void request_shutdown();

private:
    Edition mEdition;
    PerfMode mCurrentMode;
    PerfProfile mBaseProfile;
    PerfProfile mActiveProfile;

    std::atomic<bool> mRunning;
    std::atomic<bool> mGameDetected;
    std::mutex mProfileMutex;
    std::string mCurrentGame;

    int mEpollFd;
    int mInotifyFd;
    int mProcWatchFd;
    int mTimerFd;

    Edition detect_edition();
    void apply_base_profile();
    void apply_gaming_profile(const std::string& package);
    void revert_to_base();
    void switch_mode(PerfMode mode);

    void setup_event_loop();
    void run_event_loop();
    void cleanup_event_loop();

    void setup_proc_monitor();
    void handle_proc_event();
    void handle_timer_event();

    bool is_game_package(const std::string& package);
    std::string detect_foreground_app();

    void log_profile(const char* tag, const PerfProfile& profile);
    void log_hardware_info();
    int get_cpu_count();
    long get_total_ram_mb();
    std::string get_gpu_model();
    std::string get_soc_model();
};

} // namespace scandium
