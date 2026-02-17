// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class LmkTuner {
public:
    static void apply(const LmkProfile& profile);

private:
    static void configure_lmkd_props(const LmkProfile& profile);
    static void configure_psi(const LmkProfile& profile);
    static void tune_oom_adj();
};

} // namespace scandium
