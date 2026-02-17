// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "ScandiumPerfd.h"

#include <android-base/logging.h>

int main() {
    android::base::InitLogging(nullptr, &android::base::KernelLogger);
    LOG(INFO) << "scandiumd: ScandiumUI Performance Daemon starting";

    scandium::ScandiumPerfd daemon;
    return daemon.run();
}
