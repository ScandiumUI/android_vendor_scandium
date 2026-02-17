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

const char* IoTuner::scheduler_to_string(int sched) {
    switch (sched) {
        case 0:  return "none";
        case 1:  return "cfq";
        case 2:  return "bfq";
        case 3:  return "mq-deadline";
        default: return "none";
    }
}

void IoTuner::apply(const IoProfile& profile) {
    LOG(INFO) << "scandiumd: [IO] applying I/O tuning";

    find_and_tune_devices(profile);
    tune_dm_devices(profile);
    tune_loop_devices(profile);

    LOG(INFO) << "scandiumd: [IO] tuning complete";
}

void IoTuner::tune_block_device(const char* device, const IoProfile& profile) {
    std::string base = android::base::StringPrintf("/sys/block/%s/queue", device);

    struct stat st;
    if (stat(base.c_str(), &st) != 0) return;

    const char* sched = scheduler_to_string(profile.scheduler);
    write_node((base + "/scheduler").c_str(), sched);
    write_node((base + "/read_ahead_kb").c_str(),
        std::to_string(profile.read_ahead_kb));
    write_node((base + "/nr_requests").c_str(),
        std::to_string(profile.nr_requests));
    write_node((base + "/rq_affinity").c_str(),
        std::to_string(profile.rq_affinity));
    write_node((base + "/iostats").c_str(),
        profile.iostats ? "1" : "0");
    write_node((base + "/nomerges").c_str(),
        std::to_string(profile.nomerges));
    write_node((base + "/add_random").c_str(),
        profile.add_random ? "1" : "0");
    write_node((base + "/rotational").c_str(), "0");
    std::string entropy_path = android::base::StringPrintf(
        "/sys/block/%s/queue/add_random", device);
    write_node(entropy_path.c_str(), "0");

    LOG(INFO) << "scandiumd: [IO] " << device << " sched=" << sched
              << " readahead=" << profile.read_ahead_kb << "kB"
              << " nr_req=" << profile.nr_requests
              << " rq_affinity=" << profile.rq_affinity;
}

void IoTuner::find_and_tune_devices(const IoProfile& profile) {
    const char* devices[] = {
        "sda", "sdb", "sdc", "sdd",
        "mmcblk0", "mmcblk1",
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

void IoTuner::tune_dm_devices(const IoProfile& profile) {
    for (int i = 0; i < 16; i++) {
        std::string dm = android::base::StringPrintf("dm-%d", i);
        std::string path = android::base::StringPrintf("/sys/block/%s", dm.c_str());

        struct stat st;
        if (stat(path.c_str(), &st) != 0) break;

        tune_block_device(dm.c_str(), profile);
    }
}

void IoTuner::tune_loop_devices(const IoProfile& profile) {
    for (int i = 0; i < 4; i++) {
        std::string loop = android::base::StringPrintf("loop%d", i);
        std::string path = android::base::StringPrintf("/sys/block/%s", loop.c_str());

        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;
        std::string ra_path = path + "/queue/read_ahead_kb";
        write_node(ra_path.c_str(), std::to_string(profile.read_ahead_kb));
    }
}

} // namespace scandium
