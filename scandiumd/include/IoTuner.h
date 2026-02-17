// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class IoTuner {
public:
    static void apply(const PerfProfile& profile);

private:
    static void tune_block_device(const char* device, const PerfProfile& profile);
    static void find_and_tune_devices(const PerfProfile& profile);
};

} // namespace scandium
