# Inherit mobile full common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common_mobile_full.mk)

PRODUCT_PRODUCT_PROPERTIES += \
    ro.support_one_handed_mode?=true

$(call inherit-product, vendor/scandium/config/tablet.mk)

$(call inherit-product, vendor/scandium/config/telephony.mk)

PRODUCT_PACKAGE_OVERLAYS += vendor/scandium/overlay/foldable_book
