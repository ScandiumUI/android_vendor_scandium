// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "KernelTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

bool KernelTuner::write_proc_int(const char* path, int value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return android::base::WriteStringToFile(std::to_string(value), path);
}

void KernelTuner::apply(const KernelProfile& profile) {
    LOG(INFO) << "scandiumd: [KERNEL] applying kernel tuning";

    tune_printk(profile);
    tune_hung_task(profile);
    tune_panic(profile);
    tune_entropy(profile);
    tune_realtime(profile);
    tune_aslr(profile);
    tune_perf(profile);

    LOG(INFO) << "scandiumd: [KERNEL] tuning complete";
}

void KernelTuner::tune_printk(const KernelProfile& profile) {
    std::string printk = std::to_string(profile.printk_level) + " 4 1 4";
    android::base::WriteStringToFile(printk, "/proc/sys/kernel/printk");
    struct stat st;
    if (stat("/proc/sys/kernel/printk_devkmsg", &st) == 0) {
        android::base::WriteStringToFile("off", "/proc/sys/kernel/printk_devkmsg");
    }

    LOG(INFO) << "scandiumd: [KERNEL] printk_level=" << profile.printk_level;
}

void KernelTuner::tune_hung_task(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/hung_task_timeout_secs",
        profile.hung_task_timeout);

    if (profile.hung_task_timeout == 0) {
        LOG(INFO) << "scandiumd: [KERNEL] hung_task detection disabled";
    }
}

void KernelTuner::tune_panic(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/panic_on_oops", profile.panic_on_oops);
    write_proc_int("/proc/sys/kernel/panic", 0);
    write_proc_int("/proc/sys/kernel/softlockup_panic", 0);
    write_proc_int("/proc/sys/kernel/nmi_watchdog", 0);
}

void KernelTuner::tune_entropy(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/random/read_wakeup_threshold",
        profile.entropy_read_wakeup);
    write_proc_int("/proc/sys/kernel/random/write_wakeup_threshold",
        profile.entropy_write_wakeup);
    write_proc_int("/proc/sys/kernel/random/urandom_min_reseed_secs", 60);

    LOG(INFO) << "scandiumd: [KERNEL] entropy read_wakeup="
              << profile.entropy_read_wakeup
              << " write_wakeup=" << profile.entropy_write_wakeup;
}

void KernelTuner::tune_realtime(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/sched_rt_runtime_us",
        profile.sched_rt_runtime_us);
    write_proc_int("/proc/sys/kernel/sched_rt_period_us",
        profile.sched_rt_period_us);
}

void KernelTuner::tune_aslr(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/randomize_va_space",
        profile.randomize_va_space);
}

void KernelTuner::tune_perf(const KernelProfile& profile) {
    write_proc_int("/proc/sys/kernel/perf_cpu_time_max_percent",
        profile.perf_cpu_time_max_pct);
    write_proc_int("/proc/sys/kernel/timer_migration", 1);
    write_proc_int("/proc/sys/kernel/dmesg_restrict", 1);
    write_proc_int("/proc/sys/kernel/sysrq", 0);
    struct stat st;
    if (stat("/proc/sys/kernel/core_pattern", &st) == 0) {
        android::base::WriteStringToFile("|/bin/false",
            "/proc/sys/kernel/core_pattern");
    }

    LOG(INFO) << "scandiumd: [KERNEL] perf_cpu_max=" << profile.perf_cpu_time_max_pct << "%";
}

} // namespace scandium
