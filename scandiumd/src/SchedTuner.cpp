// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "SchedTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

bool SchedTuner::write_proc_int(const char* path, int value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return android::base::WriteStringToFile(std::to_string(value), path);
}

void SchedTuner::apply(const SchedProfile& profile) {
    LOG(INFO) << "scandiumd: [SCHED] applying scheduler tuning";

    detect_scheduler_type();
    tune_cfs(profile);
    tune_rt(profile);
    tune_autogroup(profile);
    tune_perf_events(profile);

    LOG(INFO) << "scandiumd: [SCHED] tuning complete";
}

void SchedTuner::tune_cfs(const SchedProfile& profile) {
    write_proc_int("/proc/sys/kernel/sched_latency_ns",
        profile.sched_latency_ns);
    write_proc_int("/proc/sys/kernel/sched_min_granularity_ns",
        profile.sched_min_granularity_ns);
    write_proc_int("/proc/sys/kernel/sched_wakeup_granularity_ns",
        profile.sched_wakeup_granularity_ns);
    write_proc_int("/proc/sys/kernel/sched_child_runs_first",
        profile.sched_child_runs_first);
    write_proc_int("/proc/sys/kernel/sched_tunable_scaling",
        profile.sched_tunable_scaling);

    LOG(INFO) << "scandiumd: [SCHED] CFS: latency=" << profile.sched_latency_ns / 1000000
              << "ms granularity=" << profile.sched_min_granularity_ns / 1000000
              << "ms wakeup=" << profile.sched_wakeup_granularity_ns / 1000000 << "ms";
}

void SchedTuner::tune_rt(const SchedProfile& profile) {
    // Do not exceed 98% CPU for RT tasks to prevent lockup
    write_proc_int("/proc/sys/kernel/sched_rt_runtime_us",
        profile.sched_rt_runtime_us);
    write_proc_int("/proc/sys/kernel/sched_rt_period_us",
        profile.sched_rt_period_us);

    LOG(INFO) << "scandiumd: [SCHED] RT: runtime=" << profile.sched_rt_runtime_us
              << "us period=" << profile.sched_rt_period_us << "us";
}

void SchedTuner::tune_autogroup(const SchedProfile& profile) {
    write_proc_int("/proc/sys/kernel/sched_autogroup_enabled",
        profile.sched_autogroup_enabled);

    LOG(INFO) << "scandiumd: [SCHED] autogroup="
              << (profile.sched_autogroup_enabled ? "enabled" : "disabled");
}

void SchedTuner::tune_perf_events(const SchedProfile& profile) {
    write_proc_int("/proc/sys/kernel/perf_event_max_sample_rate",
        profile.kernel_perf_event_max_sample_rate);

    // Disable perf paranoid for better perf stats
    write_proc_int("/proc/sys/kernel/perf_event_paranoid", -1);
}

void SchedTuner::detect_scheduler_type() {
    // Check if WALT or PELT is used
    struct stat st;
    if (stat("/proc/sys/kernel/sched_walt_rotate_big_tasks", &st) == 0) {
        LOG(INFO) << "scandiumd: [SCHED] detected WALT scheduler";
    } else if (stat("/proc/sys/kernel/sched_pelt_multiplier", &st) == 0) {
        LOG(INFO) << "scandiumd: [SCHED] detected PELT scheduler";
    } else {
        LOG(INFO) << "scandiumd: [SCHED] standard CFS scheduler";
    }

    // Check for EAS
    if (stat("/proc/sys/kernel/sched_energy_aware", &st) == 0) {
        std::string eas;
        if (android::base::ReadFileToString("/proc/sys/kernel/sched_energy_aware", &eas)) {
            eas.erase(eas.find_last_not_of("\n\r ") + 1);
            LOG(INFO) << "scandiumd: [SCHED] EAS=" << eas;
        }
    }
}

} // namespace scandium
