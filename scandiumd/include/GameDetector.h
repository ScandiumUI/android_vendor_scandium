// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scandium {

class GameDetector {
public:
    GameDetector();
    ~GameDetector();

    void load_game_database();
    bool is_game(const std::string& package_name) const;
    PerfMode get_game_mode(const std::string& package_name) const;
    int get_game_priority(const std::string& package_name) const;

    std::string detect_foreground_package();
    bool check_foreground_changed(std::string& out_package);

    int get_inotify_fd() const { return mInotifyFd; }
    void handle_inotify_event();

private:
    std::unordered_map<std::string, GameEntry> mGameDb;
    std::unordered_set<std::string> mKnownGames;
    std::string mLastForeground;

    int mInotifyFd;
    int mProcWd;

    void add_game(const char* pkg, PerfMode mode, int priority);
    std::string read_cmdline(pid_t pid);
    pid_t find_foreground_pid();
    std::string read_oom_adj(pid_t pid);
};

} // namespace scandium
