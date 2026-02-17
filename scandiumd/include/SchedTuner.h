// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class SchedTuner {
public:
    static void apply(const SchedProfile& profile);

private:
    static void tune_cfs(const SchedProfile& profile);
    static void tune_rt(const SchedProfile& profile);
    static void tune_autogroup(const SchedProfile& profile);
    static void tune_perf_events(const SchedProfile& profile);
    static void detect_scheduler_type();
    static bool write_proc_int(const char* path, int value);
};

} // namespace scandium
