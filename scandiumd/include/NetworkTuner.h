// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PerfProfile.h"

#include <string>

namespace scandium {

class NetworkTuner {
public:
    static void apply(const NetProfile& profile);

private:
    static void tune_tcp_buffers(const NetProfile& profile);
    static void tune_tcp_congestion(const NetProfile& profile);
    static void tune_tcp_keepalive(const NetProfile& profile);
    static void tune_tcp_tweaks(const NetProfile& profile);
    static void tune_core_net(const NetProfile& profile);
    static void tune_dns_cache();
    static void disable_ipv6_privacy();
    static bool write_proc(const char* path, const std::string& value);
    static bool write_proc_int(const char* path, int value);
};

} // namespace scandium
