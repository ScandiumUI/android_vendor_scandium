// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "InterruptBalancer.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <string>

namespace scandium {

bool InterruptBalancer::write_sysfs(const char* path, const std::string& value) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return android::base::WriteStringToFile(value, path);
}

std::string InterruptBalancer::mask_to_hex(int mask) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%x", mask);
    return std::string(buf);
}

void InterruptBalancer::apply(const IrqProfile& profile) {
    LOG(INFO) << "scandiumd: [IRQ] applying interrupt balancing";

    if (profile.balance_irqs) {
        balance_irq_affinity(profile.rps_cpus_mask);
    }

    configure_rps(profile.rps_cpus_mask);
    configure_xps(profile.xps_cpus_mask);

    if (profile.rfs_enabled) {
        configure_rfs(profile);
    }

    LOG(INFO) << "scandiumd: [IRQ] balancing complete";
}

void InterruptBalancer::balance_irq_affinity(int cpu_mask) {
    DIR* dir = opendir("/proc/irq");
    if (!dir) return;

    std::string mask_hex = mask_to_hex(cpu_mask);
    int balanced = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (!isdigit(entry->d_name[0])) continue;

        int irq_num = atoi(entry->d_name);
        if (irq_num <= 0) continue;

        std::string affinity_path = android::base::StringPrintf(
            "/proc/irq/%d/smp_affinity", irq_num);

        struct stat st;
        if (stat(affinity_path.c_str(), &st) != 0) continue;

        if (android::base::WriteStringToFile(mask_hex, affinity_path)) {
            balanced++;
        }
    }
    closedir(dir);

    LOG(INFO) << "scandiumd: [IRQ] balanced " << balanced
              << " IRQs to mask 0x" << mask_hex;
}

void InterruptBalancer::configure_rps(int cpu_mask) {
    const char* net_devices[] = {
        "wlan0", "rmnet0", "rmnet_data0", "eth0",
        "rmnet_ipa0", "dummy0",
        nullptr
    };

    std::string mask_hex = mask_to_hex(cpu_mask);

    struct stat st;
    for (int i = 0; net_devices[i]; i++) {
        std::string rps_path = android::base::StringPrintf(
            "/sys/class/net/%s/queues/rx-0/rps_cpus", net_devices[i]);

        if (stat(rps_path.c_str(), &st) != 0) continue;

        write_sysfs(rps_path.c_str(), mask_hex);

        std::string flow_path = android::base::StringPrintf(
            "/sys/class/net/%s/queues/rx-0/rps_flow_cnt", net_devices[i]);
        if (stat(flow_path.c_str(), &st) == 0) {
            write_sysfs(flow_path.c_str(), "256");
        }

        LOG(INFO) << "scandiumd: [IRQ] RPS " << net_devices[i]
                  << " mask=0x" << mask_hex;
    }
}

void InterruptBalancer::configure_xps(int cpu_mask) {
    // XPS (Transmit Packet Steering) — distribute network TX across CPUs
    const char* net_devices[] = {
        "wlan0", "rmnet0", "rmnet_data0", "eth0", nullptr
    };

    std::string mask_hex = mask_to_hex(cpu_mask);

    struct stat st;
    for (int i = 0; net_devices[i]; i++) {
        std::string xps_path = android::base::StringPrintf(
            "/sys/class/net/%s/queues/tx-0/xps_cpus", net_devices[i]);

        if (stat(xps_path.c_str(), &st) != 0) continue;
        write_sysfs(xps_path.c_str(), mask_hex);
    }
}

void InterruptBalancer::configure_rfs(const IrqProfile& profile) {
    // RFS (Receive Flow Steering) — steer packets to CPU processing the socket
    android::base::WriteStringToFile(
        std::to_string(profile.rfs_flow_entries),
        "/proc/sys/net/core/rps_sock_flow_entries");

    LOG(INFO) << "scandiumd: [IRQ] RFS flow_entries=" << profile.rfs_flow_entries;
}

void InterruptBalancer::set_irq_affinity(int irq_num, int cpu_mask) {
    std::string path = android::base::StringPrintf(
        "/proc/irq/%d/smp_affinity", irq_num);
    write_sysfs(path.c_str(), mask_to_hex(cpu_mask));
}

int InterruptBalancer::count_irqs() {
    int count = 0;
    DIR* dir = opendir("/proc/irq");
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (isdigit(entry->d_name[0])) count++;
    }
    closedir(dir);
    return count;
}

} // namespace scandium
