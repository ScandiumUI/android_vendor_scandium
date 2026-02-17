// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class IoTuner {
public:
    static void apply(const IoProfile& profile);

private:
    static void tune_block_device(const char* device, const IoProfile& profile);
    static void find_and_tune_devices(const IoProfile& profile);
    static void tune_dm_devices(const IoProfile& profile);
    static void tune_loop_devices(const IoProfile& profile);
    static const char* scheduler_to_string(int sched);
};

} // namespace scandium
