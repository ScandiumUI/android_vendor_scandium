// Copyright (C) 2024-2026 The ScandiumUI Project
// SPDX-License-Identifier: Apache-2.0
//
// GPU Spoof Module — "Rata Kanan"

#include "GpuSpoof.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

namespace scandium {

void GpuSpoof::apply(Edition edition) {
    LOG(INFO) << "scandiumd: [SPOOF] applying GPU spoof ["
              << edition_to_string(edition) << "]";

    force_gpu_rendering();
    spoof_renderer_props(edition);
    spoof_gles_version(edition);
    force_msaa(edition);
    set_vulkan_props(edition);
    configure_shader_cache(edition);
    configure_texture_quality(edition);

    LOG(INFO) << "scandiumd: [SPOOF] complete";
}

void GpuSpoof::force_gpu_rendering() {
    android::base::SetProperty("persist.sys.ui.hw", "1");
    android::base::SetProperty("debug.egl.hw", "1");
    android::base::SetProperty("debug.composition.type", "gpu");
    android::base::SetProperty("debug.egl.profiler", "1");
    android::base::SetProperty("persist.sys.gpu.rendering", "true");
    android::base::SetProperty("debug.sf.hw", "1");
    android::base::SetProperty("debug.sf.gpu_comp_tiling", "1");
    android::base::SetProperty("debug.enabletr", "true");
    android::base::SetProperty("renderthread.skia.reduceopstasksplitting", "true");
    android::base::SetProperty("debug.renderengine.backend", "skiaglthreaded");

    LOG(INFO) << "scandiumd: [SPOOF] forced GPU rendering";
}

void GpuSpoof::spoof_renderer_props(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            break;

        case Edition::CASUAL:
            android::base::SetProperty("persist.scandium.gpu.spoof.renderer",
                "Adreno (TM) 730");
            android::base::SetProperty("persist.scandium.gpu.spoof.vendor",
                "Qualcomm");
            android::base::SetProperty("debug.hwui.profile", "true");
            android::base::SetProperty("debug.hwui.overdraw", "false");
            break;

        case Edition::PROFESSIONAL:
            android::base::SetProperty("persist.scandium.gpu.spoof.renderer",
                "Adreno (TM) 750");
            android::base::SetProperty("persist.scandium.gpu.spoof.vendor",
                "Qualcomm");
            android::base::SetProperty("debug.hwui.profile", "true");
            android::base::SetProperty("debug.hwui.overdraw", "false");
            android::base::SetProperty("debug.egl.traceGpuCompletion", "1");
            break;
    }
}

void GpuSpoof::spoof_gles_version(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            break;
        case Edition::CASUAL:
            android::base::SetProperty("ro.opengles.version", "196609");
            break;
        case Edition::PROFESSIONAL:
            android::base::SetProperty("ro.opengles.version", "196610");
            break;
    }
}

void GpuSpoof::force_msaa(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            android::base::SetProperty("debug.egl.force_msaa", "false");
            android::base::SetProperty("persist.scandium.gpu.msaa", "0");
            break;

        case Edition::CASUAL:
            android::base::SetProperty("debug.egl.force_msaa", "true");
            android::base::SetProperty("persist.scandium.gpu.msaa", "2");
            break;

        case Edition::PROFESSIONAL:
            android::base::SetProperty("debug.egl.force_msaa", "true");
            android::base::SetProperty("persist.scandium.gpu.msaa", "4");
            android::base::SetProperty("persist.scandium.gpu.aniso_filter", "16");
            android::base::SetProperty("debug.sf.disable_client_composition_cache", "1");
            break;
    }
}

void GpuSpoof::set_vulkan_props(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            android::base::SetProperty("debug.hwui.renderer", "skiagl");
            android::base::SetProperty("persist.scandium.gpu.vulkan", "default");
            break;

        case Edition::CASUAL:
            android::base::SetProperty("debug.hwui.renderer", "skiavk");
            android::base::SetProperty("persist.scandium.gpu.vulkan", "auto");
            break;

        case Edition::PROFESSIONAL:
            android::base::SetProperty("debug.hwui.renderer", "skiavk");
            android::base::SetProperty("persist.scandium.gpu.vulkan", "force");
            android::base::SetProperty("debug.vulkan.layers", "");
            android::base::SetProperty("persist.graphics.vulkan.disable", "false");
            android::base::SetProperty("persist.scandium.gpu.vulkan.deferred", "true");
            break;
    }
}

void GpuSpoof::configure_shader_cache(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            android::base::SetProperty("persist.scandium.gpu.shader_cache", "false");
            break;
        case Edition::CASUAL:
            android::base::SetProperty("persist.scandium.gpu.shader_cache", "true");
            android::base::SetProperty("debug.hwui.use_hint_manager", "true");
            break;
        case Edition::PROFESSIONAL:
            android::base::SetProperty("persist.scandium.gpu.shader_cache", "true");
            android::base::SetProperty("debug.hwui.use_hint_manager", "true");
            android::base::SetProperty("debug.hwui.target_cpu_time_percent", "60");
            android::base::SetProperty("debug.hwui.skia_atrace_enabled", "false");
            break;
    }
}

void GpuSpoof::configure_texture_quality(Edition edition) {
    switch (edition) {
        case Edition::ACADEMY:
            android::base::SetProperty("persist.scandium.gpu.texture_quality", "default");
            break;
        case Edition::CASUAL:
            android::base::SetProperty("persist.scandium.gpu.texture_quality", "high");
            android::base::SetProperty("debug.hwui.render_dirty_regions", "false");
            break;
        case Edition::PROFESSIONAL:
            android::base::SetProperty("persist.scandium.gpu.texture_quality", "ultra");
            android::base::SetProperty("debug.hwui.render_dirty_regions", "false");
            android::base::SetProperty("debug.hwui.use_gpu_pixel_buffers", "true");
            android::base::SetProperty("persist.scandium.gpu.aniso_filter", "16");
            break;
    }
}

} // namespace scandium
