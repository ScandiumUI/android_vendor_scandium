// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "LmkTuner.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/file.h>

#include <string>

namespace scandium {

void LmkTuner::apply(const LmkProfile& profile) {
    LOG(INFO) << "scandiumd: [LMK] applying LMKD tuning";

    configure_lmkd_props(profile);
    configure_psi(profile);
    tune_oom_adj();

    LOG(INFO) << "scandiumd: [LMK] tuning complete";
}

void LmkTuner::configure_lmkd_props(const LmkProfile& profile) {
    android::base::SetProperty("ro.lmk.kill_heaviest_task",
        profile.kill_heaviest ? "true" : "false");
    android::base::SetProperty("ro.lmk.thrashing_limit",
        std::to_string(profile.thrashing_limit));
    android::base::SetProperty("ro.lmk.swap_free_low_percentage",
        std::to_string(profile.swap_free_low_pct));
    android::base::SetProperty("ro.lmk.psi_complete_stall_ms",
        std::to_string(profile.psi_complete_stall_ms));
    android::base::SetProperty("ro.lmk.psi_partial_stall_ms",
        std::to_string(profile.psi_partial_stall_ms));

    android::base::SetProperty("ro.lmk.use_psi", "true");
    android::base::SetProperty("ro.lmk.use_minfree_levels", "true");
    android::base::SetProperty("ro.lmk.kill_timeout_ms", "100");
    android::base::SetProperty("ro.lmk.debug", "false");
    android::base::SetProperty("ro.lmk.thrashing_limit_decay",
        std::to_string(profile.thrashing_limit * 2 / 3));

    LOG(INFO) << "scandiumd: [LMK] kill_heaviest=" << profile.kill_heaviest
              << " thrashing=" << profile.thrashing_limit
              << " swap_low=" << profile.swap_free_low_pct << "%"
              << " psi_complete=" << profile.psi_complete_stall_ms << "ms";
}

void LmkTuner::configure_psi(const LmkProfile& profile) {
    android::base::SetProperty("ro.lmk.psi_scrit_complete_stall_ms",
        std::to_string(profile.psi_complete_stall_ms));
    android::base::SetProperty("ro.lmk.psi_scrit_partial_stall_ms",
        std::to_string(profile.psi_partial_stall_ms));

    int low_pct = profile.swap_free_low_pct;
    android::base::SetProperty("ro.lmk.swap_util_max",
        std::to_string(100 - low_pct));
}

void LmkTuner::tune_oom_adj() {
    android::base::SetProperty("persist.scandium.lmk.oom_adj_foreground", "0");
    android::base::SetProperty("persist.scandium.lmk.oom_adj_visible", "100");
    android::base::SetProperty("persist.scandium.lmk.oom_adj_perceptible", "200");
    android::base::SetProperty("persist.scandium.lmk.oom_adj_cached", "900");
}

} // namespace scandium
