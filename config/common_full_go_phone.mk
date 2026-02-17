# Set ScandiumUI specific identifier for Android Go enabled products
PRODUCT_TYPE := go

# Inherit full common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common_full_phone.mk)
