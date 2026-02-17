// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0

#include "DisplayTuner.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <sys/stat.h>
#include <string>

namespace scandium {

void DisplayTuner::apply(const DisplayProfile& profile) {
    LOG(INFO) << "scandiumd: [DISPLAY] applying display tuning";

    detect_display_capabilities();
    set_refresh_rate(profile.preferred_refresh_rate);
    set_phase_offsets(profile);
    configure_surfaceflinger(profile);
    configure_hwc(profile);

    LOG(INFO) << "scandiumd: [DISPLAY] tuning complete";
}

void DisplayTuner::apply_touch_boost(int duration_ms) {
    if (duration_ms <= 0) return;

    android::base::SetProperty("persist.scandium.display.touch_boost_ms",
        std::to_string(duration_ms));
    android::base::SetProperty("persist.scandium.display.touch_boost_active", "true");

    LOG(INFO) << "scandiumd: [DISPLAY] touch boost=" << duration_ms << "ms";
}

void DisplayTuner::set_refresh_rate(int rate) {
    android::base::SetProperty("persist.scandium.display.refresh_rate",
        std::to_string(rate));
    android::base::SetProperty("vendor.display.idle_time", "0");
    android::base::SetProperty("vendor.display.idle_time_inactive", "0");

    switch (rate) {
        case 120:
            android::base::SetProperty("ro.surface_flinger.set_idle_timer_ms", "0");
            android::base::SetProperty("ro.surface_flinger.set_touch_timer_ms", "5000");
            android::base::SetProperty("ro.surface_flinger.use_content_detection_for_refresh_rate", "false");
            break;
        case 90:
            android::base::SetProperty("ro.surface_flinger.set_idle_timer_ms", "3000");
            android::base::SetProperty("ro.surface_flinger.set_touch_timer_ms", "3000");
            android::base::SetProperty("ro.surface_flinger.use_content_detection_for_refresh_rate", "true");
            break;
        case 60:
        default:
            android::base::SetProperty("ro.surface_flinger.set_idle_timer_ms", "1000");
            android::base::SetProperty("ro.surface_flinger.set_touch_timer_ms", "1000");
            android::base::SetProperty("ro.surface_flinger.use_content_detection_for_refresh_rate", "true");
            break;
    }

    LOG(INFO) << "scandiumd: [DISPLAY] refresh_rate=" << rate << "Hz";
}

void DisplayTuner::set_phase_offsets(const DisplayProfile& profile) {
    android::base::SetProperty("debug.sf.phase_offset_threshold_for_next_vsync_ns",
        std::to_string(profile.sf_phase_offset_ns));
    android::base::SetProperty("debug.sf.early_phase_offset_ns",
        std::to_string(profile.sf_phase_offset_ns));
    android::base::SetProperty("debug.sf.early_gl_phase_offset_ns",
        std::to_string(profile.sf_phase_offset_ns));
    android::base::SetProperty("debug.sf.early_app_phase_offset_ns",
        std::to_string(profile.vsync_phase_offset_ns));
    android::base::SetProperty("debug.sf.early_gl_app_phase_offset_ns",
        std::to_string(profile.vsync_phase_offset_ns));

    if (profile.present_time_offset_ns > 0) {
        android::base::SetProperty("debug.sf.present_time_offset_from_vsync_ns",
            std::to_string(profile.present_time_offset_ns));
    }
}

void DisplayTuner::configure_surfaceflinger(const DisplayProfile& profile) {
    android::base::SetProperty("debug.sf.latch_unsignaled",
        profile.disable_backpressure ? "1" : "0");
    android::base::SetProperty("debug.sf.disable_backpressure",
        profile.disable_backpressure ? "1" : "0");
    android::base::SetProperty("debug.sf.enable_hwc_vds",
        profile.hw_vsync ? "1" : "0");
    android::base::SetProperty("debug.sf.gpu_comp_tiling",
        std::to_string(profile.gpu_comp_tiling));

    android::base::SetProperty("ro.surface_flinger.max_frame_buffer_acquired_buffers", "3");
    android::base::SetProperty("debug.sf.enable_hwc_vds", "1");
}

void DisplayTuner::configure_hwc(const DisplayProfile& profile) {
    android::base::SetProperty("debug.sf.enable_gl_backpressure",
        profile.disable_backpressure ? "0" : "1");
    android::base::SetProperty("vendor.gralloc.disable_ubwc", "0");
    android::base::SetProperty("persist.sys.sf.color_mode", "0");
    android::base::SetProperty("persist.sys.sf.color_saturation", "1.0");
}

void DisplayTuner::detect_display_capabilities() {
    const char* panel_paths[] = {
        "/sys/class/graphics/fb0/msm_fb_panel_info",
        "/sys/class/drm/card0-DSI-1/modes",
        "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/card0-DSI-1/modes",
        nullptr
    };

    struct stat st;
    for (int i = 0; panel_paths[i]; i++) {
        if (stat(panel_paths[i], &st) == 0) {
            std::string info;
            if (android::base::ReadFileToString(panel_paths[i], &info)) {
                info.erase(info.find_last_not_of("\n\r ") + 1);
                LOG(INFO) << "scandiumd: [DISPLAY] panel: " << info.substr(0, 100);
            }
            break;
        }
    }

    const char* brightness_paths[] = {
        "/sys/class/backlight/panel0-backlight/max_brightness",
        "/sys/class/leds/lcd-backlight/max_brightness",
        nullptr
    };

    for (int i = 0; brightness_paths[i]; i++) {
        if (stat(brightness_paths[i], &st) == 0) {
            std::string max_br;
            if (android::base::ReadFileToString(brightness_paths[i], &max_br)) {
                max_br.erase(max_br.find_last_not_of("\n\r ") + 1);
                LOG(INFO) << "scandiumd: [DISPLAY] max_brightness=" << max_br;
            }
            break;
        }
    }
}

} // namespace scandium
