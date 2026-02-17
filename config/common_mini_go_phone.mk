# Set ScandiumUI specific identifier for Android Go enabled products
PRODUCT_TYPE := go

# Inherit mini common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common_mini_phone.mk)
