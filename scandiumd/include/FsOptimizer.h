// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

#include <string>

namespace scandium {

class FsOptimizer {
public:
    static void apply(const FsProfile& profile);

private:
    static void tune_f2fs(const FsProfile& profile);
    static void tune_ext4(const FsProfile& profile);
    static void tune_inotify_limits(const FsProfile& profile);
    static void tune_dentry_cache();
    static void detect_filesystem_type(std::string& fs_type, std::string& mount_point);
    static bool write_sysfs(const char* path, const std::string& value);
    static bool has_f2fs();
    static bool has_ext4();
};

} // namespace scandium
