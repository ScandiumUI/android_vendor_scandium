// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class DisplayTuner {
public:
    static void apply(const DisplayProfile& profile);
    static void apply_touch_boost(int duration_ms);

private:
    static void set_refresh_rate(int rate);
    static void set_phase_offsets(const DisplayProfile& profile);
    static void configure_surfaceflinger(const DisplayProfile& profile);
    static void configure_hwc(const DisplayProfile& profile);
    static void detect_display_capabilities();
};

} // namespace scandium
