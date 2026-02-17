#
# Copyright (C) 2024-2026 The ScandiumUI Project
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.dex2oat-threads=2 \
    dalvik.vm.image-dex2oat-threads=2 \
    dalvik.vm.dex2oat-filter=quicken \
    dalvik.vm.image-dex2oat-filter=speed-profile \
    pm.dexopt.install=quicken \
    pm.dexopt.bg-dexopt=speed-profile \
    pm.dexopt.boot-after-ota=verify \
    pm.dexopt.first-boot=verify

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapgrowthlimit=128m \
    dalvik.vm.heapsize=256m \
    dalvik.vm.heaptargetutilization=0.80 \
    dalvik.vm.heapminfree=2m \
    dalvik.vm.heapmaxfree=8m

PRODUCT_PRODUCT_PROPERTIES += \
    ro.lmk.kill_heaviest_task=true \
    ro.lmk.thrashing_limit=60 \
    ro.lmk.psi_complete_stall_ms=300

PRODUCT_PACKAGES += \
    Etar \
    Recorder

ifneq ($(PRODUCT_NO_CAMERA),true)
PRODUCT_PACKAGES += \
    Aperture
endif

PRODUCT_PACKAGES += \
    Glimpse

$(call inherit-product-if-exists, external/google-fonts/google-sans-flex/fonts.mk)

PRODUCT_PACKAGES += \
    fonts_customization.xml \
    FontGoogleSansFlexOverlay

PRODUCT_PACKAGES += \
    ThemesStub

ifeq ($(PRODUCT_TYPE), go)
PRODUCT_PACKAGES += \
    Launcher3QuickStepGo

PRODUCT_DEXPREOPT_SPEED_APPS += \
    Launcher3QuickStepGo
else
PRODUCT_PACKAGES += \
    Launcher3QuickStep

PRODUCT_DEXPREOPT_SPEED_APPS += \
    Launcher3QuickStep
endif

PRODUCT_PRODUCT_PROPERTIES += \
    persist.scandium.feature.audiofx=false \
    persist.scandium.feature.profiles=false \
    persist.scandium.feature.theming=false \
    persist.scandium.feature.recorder=true \
    persist.scandium.feature.gallery=true \
    persist.scandium.feature.extra_tools=false

PRODUCT_PACKAGES += scandiumd
PRODUCT_PRODUCT_PROPERTIES += \
    persist.scandium.perf.level=efficient \
    persist.scandium.daemon.persistent=false \
    persist.scandium.gpu.spoof.enabled=false \
    persist.scandium.gpu.msaa=0 \
    persist.scandium.gpu.texture_quality=default \
    persist.scandium.gpu.vulkan=default \
    debug.hwui.renderer=skiagl \
    persist.scandium.net.tcp_congestion=cubic \
    persist.scandium.net.tcp_fastopen=false \
    persist.scandium.display.refresh_rate=60 \
    persist.scandium.display.touch_boost_ms=0 \
    persist.scandium.sched.autogroup=1 \
    persist.scandium.kernel.printk_level=4 \
    persist.scandium.irq.balance=false \
    persist.scandium.irq.rfs=false \
    persist.scandium.fs.f2fs_gc=true \
    persist.scandium.thermal.threshold=38 \
    persist.scandium.thermal.adaptive=false \
    persist.scandium.game.detection=false
