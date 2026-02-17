# Inherit common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common.mk)

# Include AOSP audio files
$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioTv.mk)

# Inherit ScandiumUI atv device tree
$(call inherit-product, device/scandium/atv/scandium_atv.mk)

# AOSP packages
PRODUCT_PACKAGES += \
    LeanbackIME

# ScandiumUI packages
PRODUCT_PACKAGES += \
    Catapult \
    ScandiumCustomizer

PRODUCT_PACKAGE_OVERLAYS += vendor/scandium/overlay/tv
