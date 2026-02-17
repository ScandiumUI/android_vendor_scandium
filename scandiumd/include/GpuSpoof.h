// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class GpuSpoof {
public:
    static void apply(Edition edition);

private:
    static void spoof_renderer_props(Edition edition);
    static void spoof_gles_version(Edition edition);
    static void force_gpu_rendering();
    static void force_4x_msaa(Edition edition);
    static void set_vulkan_props(Edition edition);
};

} // namespace scandium
