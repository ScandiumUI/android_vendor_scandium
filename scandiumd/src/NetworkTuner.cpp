// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "NetworkTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <string>

namespace scandium {

bool NetworkTuner::write_proc(const char* path, const std::string& value) {
    if (!android::base::WriteStringToFile(value, path)) {
        LOG(WARNING) << "scandiumd: [NET] write failed: " << path;
        return false;
    }
    return true;
}

bool NetworkTuner::write_proc_int(const char* path, int value) {
    return write_proc(path, std::to_string(value));
}

void NetworkTuner::apply(const NetProfile& profile) {
    LOG(INFO) << "scandiumd: [NET] applying network tuning";

    tune_tcp_buffers(profile);
    tune_tcp_congestion(profile);
    tune_tcp_keepalive(profile);
    tune_tcp_tweaks(profile);
    tune_core_net(profile);
    tune_dns_cache();

    LOG(INFO) << "scandiumd: [NET] tuning complete";
}

void NetworkTuner::tune_tcp_buffers(const NetProfile& profile) {
    // TCP receive buffer
    std::string rmem = android::base::StringPrintf("%d\t%d\t%d",
        profile.tcp_rmem_min, profile.tcp_rmem_default, profile.tcp_rmem_max);
    write_proc("/proc/sys/net/ipv4/tcp_rmem", rmem);

    // TCP send buffer
    std::string wmem = android::base::StringPrintf("%d\t%d\t%d",
        profile.tcp_wmem_min, profile.tcp_wmem_default, profile.tcp_wmem_max);
    write_proc("/proc/sys/net/ipv4/tcp_wmem", wmem);

    // Core rmem/wmem max
    write_proc_int("/proc/sys/net/core/rmem_max", profile.tcp_rmem_max);
    write_proc_int("/proc/sys/net/core/wmem_max", profile.tcp_wmem_max);
    write_proc_int("/proc/sys/net/core/rmem_default", profile.tcp_rmem_default);
    write_proc_int("/proc/sys/net/core/wmem_default", profile.tcp_wmem_default);
    write_proc_int("/proc/sys/net/core/optmem_max", profile.optmem_max);

    LOG(INFO) << "scandiumd: [NET] rmem=" << profile.tcp_rmem_min << "/"
              << profile.tcp_rmem_default << "/" << profile.tcp_rmem_max
              << " wmem=" << profile.tcp_wmem_min << "/"
              << profile.tcp_wmem_default << "/" << profile.tcp_wmem_max;
}

void NetworkTuner::tune_tcp_congestion(const NetProfile& profile) {
    write_proc("/proc/sys/net/ipv4/tcp_congestion_control", profile.tcp_congestion);

    // Check if BBR is available
    std::string available;
    if (android::base::ReadFileToString(
            "/proc/sys/net/ipv4/tcp_available_congestion_control", &available)) {
        if (profile.tcp_congestion == "bbr" &&
            available.find("bbr") == std::string::npos) {
            LOG(WARNING) << "scandiumd: [NET] BBR not available, falling back to cubic";
            write_proc("/proc/sys/net/ipv4/tcp_congestion_control", "cubic");
        }
    }

    LOG(INFO) << "scandiumd: [NET] congestion=" << profile.tcp_congestion;
}

void NetworkTuner::tune_tcp_keepalive(const NetProfile& profile) {
    write_proc_int("/proc/sys/net/ipv4/tcp_keepalive_time", profile.tcp_keepalive_time);
    write_proc_int("/proc/sys/net/ipv4/tcp_keepalive_intvl", profile.tcp_keepalive_intvl);
    write_proc_int("/proc/sys/net/ipv4/tcp_keepalive_probes", profile.tcp_keepalive_probes);
}

void NetworkTuner::tune_tcp_tweaks(const NetProfile& profile) {
    write_proc_int("/proc/sys/net/ipv4/tcp_fin_timeout", profile.tcp_fin_timeout);

    write_proc_int("/proc/sys/net/ipv4/tcp_tw_reuse",
        profile.tcp_tw_reuse ? 1 : 0);
    write_proc_int("/proc/sys/net/ipv4/tcp_sack",
        profile.tcp_sack ? 1 : 0);
    write_proc_int("/proc/sys/net/ipv4/tcp_timestamps",
        profile.tcp_timestamps ? 1 : 0);
    write_proc_int("/proc/sys/net/ipv4/tcp_window_scaling",
        profile.tcp_window_scaling ? 1 : 0);

    // TCP Fast Open
    if (profile.tcp_fastopen) {
        write_proc_int("/proc/sys/net/ipv4/tcp_fastopen", 3);
    } else {
        write_proc_int("/proc/sys/net/ipv4/tcp_fastopen", 0);
    }

    // Max SYN backlog
    write_proc_int("/proc/sys/net/ipv4/tcp_max_syn_backlog",
        profile.tcp_max_syn_backlog);

    // Disable slow start after idle for gaming
    write_proc_int("/proc/sys/net/ipv4/tcp_slow_start_after_idle", 0);

    // Enable MTU probing
    write_proc_int("/proc/sys/net/ipv4/tcp_mtu_probing", 1);

    // Orphan handling
    write_proc_int("/proc/sys/net/ipv4/tcp_max_orphans", 32768);

    // SYN cookies protection
    write_proc_int("/proc/sys/net/ipv4/tcp_syncookies", 1);

    // ECN
    write_proc_int("/proc/sys/net/ipv4/tcp_ecn", 0);

    LOG(INFO) << "scandiumd: [NET] fastopen=" << profile.tcp_fastopen
              << " sack=" << profile.tcp_sack
              << " timestamps=" << profile.tcp_timestamps
              << " tw_reuse=" << profile.tcp_tw_reuse;
}

void NetworkTuner::tune_core_net(const NetProfile& profile) {
    write_proc_int("/proc/sys/net/core/somaxconn", profile.somaxconn);
    write_proc_int("/proc/sys/net/core/netdev_max_backlog",
        profile.netdev_max_backlog);
    write_proc_int("/proc/sys/net/core/netdev_budget", 600);
    write_proc_int("/proc/sys/net/core/netdev_budget_usecs", 8000);

    // IPv4 routing optimizations
    write_proc_int("/proc/sys/net/ipv4/ip_no_pmtu_disc", 0);
    write_proc_int("/proc/sys/net/ipv4/route/flush", 1);

    LOG(INFO) << "scandiumd: [NET] somaxconn=" << profile.somaxconn
              << " backlog=" << profile.netdev_max_backlog;
}

void NetworkTuner::tune_dns_cache() {
    // DNS negative cache timeout
    write_proc_int("/proc/sys/net/ipv4/ipfrag_time", 30);

    // ARP cache tuning
    write_proc_int("/proc/sys/net/ipv4/neigh/default/gc_stale_time", 120);
}

void NetworkTuner::disable_ipv6_privacy() {
    write_proc_int("/proc/sys/net/ipv6/conf/all/use_tempaddr", 0);
}

} // namespace scandium
