// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "GameDetector.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <sys/inotify.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <fstream>

namespace scandium {

GameDetector::GameDetector()
    : mInotifyFd(-1),
      mProcWd(-1) {
}

GameDetector::~GameDetector() {
    if (mProcWd >= 0 && mInotifyFd >= 0) {
        inotify_rm_watch(mInotifyFd, mProcWd);
    }
    if (mInotifyFd >= 0) close(mInotifyFd);
}

void GameDetector::load_game_database() {
    auto db = get_game_database();

    for (const auto& entry : db) {
        mGameDb[entry.package_name] = entry;
        mKnownGames.insert(entry.package_name);
    }

    LOG(INFO) << "scandiumd: [GAME] loaded " << mGameDb.size()
              << " games into detection database";
}

bool GameDetector::is_game(const std::string& package_name) const {
    // Exact match
    if (mKnownGames.count(package_name)) return true;

    // Partial match (some games have variant package names)
    for (const auto& game : mKnownGames) {
        if (package_name.find(game) != std::string::npos) return true;
        if (game.find(package_name) != std::string::npos) return true;
    }

    // Heuristic: check for common game-related keywords
    static const char* game_hints[] = {
        "game", "Game", "play", "craft", "racing",
        "shooter", "strike", "battle", "legends",
        "warrior", "arena", "quest", "rpg",
        nullptr
    };
    for (int i = 0; game_hints[i]; i++) {
        if (package_name.find(game_hints[i]) != std::string::npos) return true;
    }

    return false;
}

PerfMode GameDetector::get_game_mode(const std::string& package_name) const {
    auto it = mGameDb.find(package_name);
    if (it != mGameDb.end()) return it->second.mode;
    return PerfMode::GAMING;
}

int GameDetector::get_game_priority(const std::string& package_name) const {
    auto it = mGameDb.find(package_name);
    if (it != mGameDb.end()) return it->second.priority;
    return 5;
}

std::string GameDetector::detect_foreground_package() {
    DIR* dir = opendir("/proc");
    if (!dir) return "";

    std::string best_pkg;
    pid_t best_pid = -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        if (!isdigit(entry->d_name[0])) continue;

        pid_t pid = atoi(entry->d_name);
        if (pid <= 100) continue;

        // Check oom_score_adj — foreground apps have adj 0
        std::string oom_path = android::base::StringPrintf(
            "/proc/%d/oom_score_adj", pid);
        std::string oom_str;
        if (!android::base::ReadFileToString(oom_path, &oom_str)) continue;

        int oom = atoi(oom_str.c_str());
        if (oom != 0) continue;

        // Read cmdline for package name
        std::string cmdline = read_cmdline(pid);
        if (cmdline.empty()) continue;

        // Filter system processes
        if (cmdline[0] == '/') continue;
        if (cmdline.find(':') != std::string::npos) continue;
        if (cmdline == "system_server") continue;
        if (cmdline == "zygote" || cmdline == "zygote64") continue;
        if (cmdline == "webview_zygote") continue;
        if (cmdline.find("android") == 0 && cmdline.find('.') == std::string::npos) continue;

        // Must look like a package name (has at least one dot)
        if (cmdline.find('.') == std::string::npos) continue;

        best_pkg = cmdline;
        best_pid = pid;
    }
    closedir(dir);

    return best_pkg;
}

bool GameDetector::check_foreground_changed(std::string& out_package) {
    std::string current = detect_foreground_package();
    if (current.empty()) return false;

    if (current != mLastForeground) {
        mLastForeground = current;
        out_package = current;
        return true;
    }

    return false;
}

std::string GameDetector::read_cmdline(pid_t pid) {
    std::string cmdline_path = android::base::StringPrintf("/proc/%d/cmdline", pid);
    std::string cmdline;
    if (!android::base::ReadFileToString(cmdline_path, &cmdline)) return "";

    // cmdline is null-terminated
    size_t null_pos = cmdline.find('\0');
    if (null_pos != std::string::npos) cmdline.resize(null_pos);

    return cmdline;
}

pid_t GameDetector::find_foreground_pid() {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        if (!isdigit(entry->d_name[0])) continue;

        pid_t pid = atoi(entry->d_name);
        std::string oom = read_oom_adj(pid);
        if (oom == "0") {
            closedir(dir);
            return pid;
        }
    }
    closedir(dir);
    return -1;
}

std::string GameDetector::read_oom_adj(pid_t pid) {
    std::string path = android::base::StringPrintf("/proc/%d/oom_score_adj", pid);
    std::string value;
    if (android::base::ReadFileToString(path, &value)) {
        value.erase(value.find_last_not_of("\n\r ") + 1);
        return value;
    }
    return "";
}

void GameDetector::handle_inotify_event() {
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(mInotifyFd, buf, sizeof(buf));
    (void)len;
}

void GameDetector::add_game(const char* pkg, PerfMode mode, int priority) {
    GameEntry entry;
    entry.package_name = pkg;
    entry.mode = mode;
    entry.priority = priority;
    mGameDb[pkg] = entry;
    mKnownGames.insert(pkg);
}

} // namespace scandium
