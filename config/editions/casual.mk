#
# Copyright (C) 2024-2026 The ScandiumUI Project
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.dex2oat-threads=4 \
    dalvik.vm.image-dex2oat-threads=4 \
    dalvik.vm.dex2oat-filter=speed-profile \
    dalvik.vm.image-dex2oat-filter=speed-profile \
    pm.dexopt.install=speed-profile \
    pm.dexopt.bg-dexopt=speed-profile \
    pm.dexopt.boot-after-ota=verify \
    pm.dexopt.first-boot=verify

PRODUCT_PRODUCT_PROPERTIES += \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapgrowthlimit=192m \
    dalvik.vm.heapsize=384m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=2m \
    dalvik.vm.heapmaxfree=8m

PRODUCT_PRODUCT_PROPERTIES += \
    debug.sf.latch_unsignaled=1

PRODUCT_PRODUCT_PROPERTIES += \
    ro.lmk.kill_heaviest_task=true \
    ro.lmk.thrashing_limit=30 \
    ro.lmk.psi_complete_stall_ms=200

PRODUCT_PACKAGES += \
    Etar \
    Recorder

PRODUCT_PACKAGES += \
    Profiles

ifneq ($(PRODUCT_NO_CAMERA),true)
PRODUCT_PACKAGES += \
    Aperture
endif

PRODUCT_PACKAGES += \
    Backgrounds \
    Glimpse

$(call inherit-product-if-exists, external/google-fonts/google-sans-flex/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/rubik/fonts.mk)

PRODUCT_PACKAGES += \
    fonts_customization.xml \
    FontGoogleSansFlexOverlay \
    FontRubikOverlay

PRODUCT_PACKAGES += \
    ScandiumBlackTheme \
    ThemePicker \
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
    persist.scandium.feature.profiles=true \
    persist.scandium.feature.theming=true \
    persist.scandium.feature.recorder=true \
    persist.scandium.feature.gallery=true \
    persist.scandium.feature.extra_tools=false \
    persist.scandium.perf.level=balanced

PRODUCT_PACKAGES += scandiumd
PRODUCT_PRODUCT_PROPERTIES += \
    persist.scandium.gpu.spoof.enabled=true \
    persist.scandium.gpu.spoof.renderer=Adreno\ (TM)\ 730 \
    persist.scandium.gpu.spoof.vendor=Qualcomm \
    persist.scandium.gpu.msaa=2 \
    persist.scandium.gpu.texture_quality=high \
    persist.scandium.gpu.vulkan=auto \
    debug.hwui.renderer=skiavk \
    ro.opengles.version=196609
