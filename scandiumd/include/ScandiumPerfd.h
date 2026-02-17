// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class ScandiumPerfd {
public:
    ScandiumPerfd();
    ~ScandiumPerfd() = default;

    int run();

private:
    Edition mEdition;
    PerfProfile mProfile;

    Edition detect_edition();
    void apply_profile();
    void log_profile();
};

} // namespace scandium
