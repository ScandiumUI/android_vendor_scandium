// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ScandiumPerfd.h"
#include "GameDetector.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <signal.h>

using namespace scandium;

static ScandiumPerfd* g_daemon = nullptr;

static void signal_handler(int sig) {
    if (g_daemon) g_daemon->request_shutdown();
}

int main() {
    android::base::InitLogging(nullptr, &android::base::KernelLogger);

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    ScandiumPerfd daemon;
    g_daemon = &daemon;

    int ret = daemon.run();

    g_daemon = nullptr;
    LOG(INFO) << "scandiumd: shutdown complete, exit=" << ret;
    return ret;
}
