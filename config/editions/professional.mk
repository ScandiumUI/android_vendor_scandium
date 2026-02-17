#
# Copyright (C) 2024-2026 The ScandiumUI Project
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.dex2oat-threads=4 \
    dalvik.vm.image-dex2oat-threads=4 \
    dalvik.vm.dex2oat-filter=speed \
    dalvik.vm.image-dex2oat-filter=speed-profile \
    dalvik.vm.dex2oat-cpu-set=0,1,2,3,4,5,6,7 \
    pm.dexopt.install=speed-profile \
    pm.dexopt.bg-dexopt=speed-profile \
    pm.dexopt.boot-after-ota=verify \
    pm.dexopt.first-boot=verify

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.heapstartsize=16m \
    dalvik.vm.heapgrowthlimit=256m \
    dalvik.vm.heapsize=512m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=4m \
    dalvik.vm.heapmaxfree=16m

PRODUCT_PRODUCT_PROPERTIES += \
    debug.sf.latch_unsignaled=1 \
    debug.sf.disable_backpressure=1

PRODUCT_PRODUCT_PROPERTIES += \
    ro.lmk.kill_heaviest_task=true \
    ro.lmk.thrashing_limit=30 \
    ro.lmk.swap_free_low_percentage=10 \
    ro.lmk.psi_complete_stall_ms=150

PRODUCT_PACKAGES += \
    Camelot \
    Etar \
    Recorder \
    Twelve

PRODUCT_PACKAGES += \
    Profiles

ifneq ($(TARGET_EXCLUDES_AUDIOFX),true)
PRODUCT_PACKAGES += \
    AudioFX
endif

ifneq ($(PRODUCT_NO_CAMERA),true)
PRODUCT_PACKAGES += \
    Aperture
endif

PRODUCT_PACKAGES += \
    Backgrounds \
    Glimpse

$(call inherit-product-if-exists, external/google-fonts/google-sans-flex/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/lato/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/rubik/fonts.mk)

PRODUCT_PACKAGES += \
    fonts_customization.xml \
    FontGoogleSansFlexOverlay \
    FontLatoOverlay \
    FontRubikOverlay

PRODUCT_PACKAGES += \
    ScandiumBlackTheme \
    ThemePicker \
    ThemesStub

PRODUCT_PACKAGES += \
    unrar \
    zstd

PRODUCT_PACKAGES += \
    Launcher3QuickStep

PRODUCT_DEXPREOPT_SPEED_APPS += \
    Launcher3QuickStep

PRODUCT_PRODUCT_PROPERTIES += \
    persist.scandium.feature.audiofx=true \
    persist.scandium.feature.profiles=true \
    persist.scandium.feature.theming=true \
    persist.scandium.feature.recorder=true \
    persist.scandium.feature.gallery=true \
    persist.scandium.feature.extra_tools=true \
    persist.scandium.perf.level=aggressive
