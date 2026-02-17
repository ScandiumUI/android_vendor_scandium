// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

#include <string>

namespace scandium {

class InterruptBalancer {
public:
    static void apply(const IrqProfile& profile);

private:
    static void balance_irq_affinity(int cpu_mask);
    static void configure_rps(int cpu_mask);
    static void configure_xps(int cpu_mask);
    static void configure_rfs(const IrqProfile& profile);
    static void set_irq_affinity(int irq_num, int cpu_mask);
    static int count_irqs();
    static std::string mask_to_hex(int mask);
    static bool write_sysfs(const char* path, const std::string& value);
};

} // namespace scandium
