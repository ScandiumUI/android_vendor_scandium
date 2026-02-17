# Inherit mobile full common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common_mobile_full.mk)

# Inherit tablet common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/tablet.mk)

$(call inherit-product, vendor/scandium/config/wifionly.mk)
