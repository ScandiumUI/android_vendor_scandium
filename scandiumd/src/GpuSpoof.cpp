// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0
//
// GPU Spoof Module — "Rata Kanan"
//
// Sets system properties to spoof GPU capabilities to the highest tier,
// forces GPU rendering for all apps, and enables max quality graphics flags.
// Each edition applies a different level of spoofing:
//   Academy      — minimal, no spoof (save power)
//   Casual       — balanced, force GPU rendering + medium quality
//   Professional — full spoof, 4x MSAA, max texture, Vulkan preferred

#include "GpuSpoof.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/file.h>

namespace scandium {

void GpuSpoof::apply(Edition edition) {
    LOG(INFO) << "scandiumd: applying GPU spoof [" << edition_to_string(edition) << "]";

    force_gpu_rendering();
    spoof_renderer_props(edition);
    spoof_gles_version(edition);
    force_4x_msaa(edition);
    set_vulkan_props(edition);
}

// Force hardware-accelerated rendering for all apps (rata kanan dasar)
void GpuSpoof::force_gpu_rendering() {
    android::base::SetProperty("persist.sys.ui.hw", "1");
    android::base::SetProperty("debug.egl.hw", "1");
    android::base::SetProperty("debug.composition.type", "gpu");
    android::base::SetProperty("debug.egl.profiler", "1");
    android::base::SetProperty("persist.sys.gpu.rendering", "true");

    // Force GPU composition instead of HWC overlay
    android::base::SetProperty("debug.sf.hw", "1");
    android::base::SetProperty("debug.sf.gpu_comp_tiling", "1");
    android::base::SetProperty("debug.enabletr", "true");

    // Skia GPU rendering backend
    android::base::SetProperty("debug.hwui.renderer", "skiavk");
    android::base::SetProperty("renderthread.skia.reduceopstasksplitting", "true");

    LOG(INFO) << "scandiumd: forced GPU rendering enabled";
}

// Spoof GPU renderer string to appear as high-end GPU
void GpuSpoof::spoof_renderer_props(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            // No spoof — report real GPU
            break;

        case Edition::CASUAL:
            // Mid-tier spoof — Adreno 730 class
            android::base::SetProperty("persist.scandium.gpu.spoof.renderer", "Adreno (TM) 730");
            android::base::SetProperty("persist.scandium.gpu.spoof.vendor", "Qualcomm");
            android::base::SetProperty("debug.hwui.profile", "true");

            // Texture quality
            android::base::SetProperty("persist.scandium.gpu.texture_quality", "high");
            android::base::SetProperty("debug.hwui.overdraw", "false");
            break;

        case Edition::PROFESSIONAL:
            // Top-tier spoof — Adreno 750 / Immortalis class
            android::base::SetProperty("persist.scandium.gpu.spoof.renderer", "Adreno (TM) 750");
            android::base::SetProperty("persist.scandium.gpu.spoof.vendor", "Qualcomm");
            android::base::SetProperty("debug.hwui.profile", "true");

            // Max texture quality
            android::base::SetProperty("persist.scandium.gpu.texture_quality", "ultra");
            android::base::SetProperty("debug.hwui.overdraw", "false");

            // Shader cache — keep compiled shaders for faster loading
            android::base::SetProperty("persist.scandium.gpu.shader_cache", "true");
            android::base::SetProperty("debug.egl.traceGpuCompletion", "1");
            break;
    }

    LOG(INFO) << "scandiumd: GPU renderer spoof applied";
}

// Spoof GLES version to report max capability
void GpuSpoof::spoof_gles_version(Edition edition) {
    // GLES version packed: major * 0x10000 + minor
    // 0x30002 = GLES 3.2 (highest)
    switch (edition) {
        case Edition::ACADEMY:
            // Keep default (real device caps)
            break;

        case Edition::CASUAL:
            // GLES 3.1 — good enough for most games
            android::base::SetProperty("ro.opengles.version", "196609"); // 0x30001
            break;

        case Edition::PROFESSIONAL:
            // GLES 3.2 — max spec, unlocks all shaders/effects
            android::base::SetProperty("ro.opengles.version", "196610"); // 0x30002
            break;
    }
}

// Force 4x MSAA anti-aliasing (rata kanan grafik)
void GpuSpoof::force_4x_msaa(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            // No MSAA — save GPU cycles
            android::base::SetProperty("debug.egl.force_msaa", "false");
            break;

        case Edition::CASUAL:
            // 2x MSAA — balanced quality
            android::base::SetProperty("debug.egl.force_msaa", "true");
            android::base::SetProperty("persist.scandium.gpu.msaa", "2");
            break;

        case Edition::PROFESSIONAL:
            // 4x MSAA — maximum anti-aliasing, rata kanan penuh
            android::base::SetProperty("debug.egl.force_msaa", "true");
            android::base::SetProperty("persist.scandium.gpu.msaa", "4");

            // Force anisotropic filtering
            android::base::SetProperty("persist.scandium.gpu.aniso_filter", "16");

            // Disable frame skipping — render every frame
            android::base::SetProperty("debug.sf.disable_client_composition_cache", "1");
            break;
    }

    LOG(INFO) << "scandiumd: MSAA configured for " << edition_to_string(edition);
}

// Vulkan rendering properties
void GpuSpoof::set_vulkan_props(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            // Use default OpenGL backend
            android::base::SetProperty("debug.hwui.renderer", "skiagl");
            break;

        case Edition::CASUAL:
            // Vulkan if available, fallback to GL
            android::base::SetProperty("debug.hwui.renderer", "skiavk");
            android::base::SetProperty("persist.scandium.gpu.vulkan", "auto");
            break;

        case Edition::PROFESSIONAL:
            // Force Vulkan — lower overhead, better for games
            android::base::SetProperty("debug.hwui.renderer", "skiavk");
            android::base::SetProperty("persist.scandium.gpu.vulkan", "force");
            android::base::SetProperty("debug.vulkan.layers", "");
            android::base::SetProperty("persist.graphics.vulkan.disable", "false");

            // Vulkan deferred rendering for better throughput
            android::base::SetProperty("persist.scandium.gpu.vulkan.deferred", "true");
            break;
    }

    LOG(INFO) << "scandiumd: Vulkan props set for " << edition_to_string(edition);
}

} // namespace scandium
