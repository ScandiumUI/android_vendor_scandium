// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

namespace scandium {

class MemoryTuner {
public:
    static void apply(const MemProfile& profile);
    static void drop_caches();
    static void compact_memory();
    static long get_available_ram_kb();

private:
    static void tune_vm(const MemProfile& profile);
    static void tune_zram(const MemProfile& profile);
    static void tune_watermark(const MemProfile& profile);
    static void tune_hugepages(const MemProfile& profile);
    static void tune_ksm(const MemProfile& profile);
    static long get_meminfo_field(const char* field);
};

} // namespace scandium
