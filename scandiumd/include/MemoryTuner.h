// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class MemoryTuner {
public:
    static void apply(const PerfProfile& profile);

private:
    static void tune_vm(const PerfProfile& profile);
    static void tune_zram(const PerfProfile& profile);
    static void drop_caches();
};

} // namespace scandium
