// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "IoTuner.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <dirent.h>
#include <sys/stat.h>
#include <string>

namespace scandium {

static bool write_node(const char* path, const std::string& value) {
    return android::base::WriteStringToFile(value, path);
}

void IoTuner::apply(const PerfProfile& profile) {
    LOG(INFO) << "scandiumd: applying I/O tuning";
    find_and_tune_devices(profile);
}

void IoTuner::tune_block_device(const char* device, const PerfProfile& profile) {
    std::string base = android::base::StringPrintf("/sys/block/%s/queue", device);

    struct stat st;
    if (stat(base.c_str(), &st) != 0) return;

    // Scheduler
    const char* sched;
    switch (profile.io_scheduler) {
        case 0:  sched = "none";        break;
        case 1:  sched = "cfq";         break;
        case 2:  sched = "bfq";         break;
        case 3:  sched = "mq-deadline"; break;
        default: sched = "none";        break;
    }
    write_node((base + "/scheduler").c_str(), sched);

    // Read-ahead
    write_node((base + "/read_ahead_kb").c_str(), std::to_string(profile.io_read_ahead_kb));

    // Nr requests
    write_node((base + "/nr_requests").c_str(), std::to_string(profile.io_nr_requests));

    // Disable I/O stats for performance (less overhead)
    if (profile.io_read_ahead_kb >= 1024) {
        write_node((base + "/iostats").c_str(), "0");
    }

    // Rotational hint (always 0 for flash storage)
    write_node((base + "/rotational").c_str(), "0");

    // RQ affinity
    write_node((base + "/rq_affinity").c_str(),
               profile.io_scheduler == 3 ? "2" : "1");

    LOG(INFO) << "scandiumd: io " << device << " sched=" << sched
              << " read_ahead=" << profile.io_read_ahead_kb << "kb";
}

void IoTuner::find_and_tune_devices(const PerfProfile& profile) {
    // Tune common block devices
    const char* devices[] = {
        "sda", "sdb", "sdc",
        "mmcblk0", "mmcblk1",
        "dm-0", "dm-1",
        nullptr
    };

    struct stat st;
    for (int i = 0; devices[i]; i++) {
        std::string path = android::base::StringPrintf("/sys/block/%s", devices[i]);
        if (stat(path.c_str(), &st) == 0) {
            tune_block_device(devices[i], profile);
        }
    }
}

} // namespace scandium
