# Inherit common ScandiumUI stuff
$(call inherit-product, vendor/scandium/config/common_mobile.mk)

PRODUCT_SIZE := full

# Include ScandiumUI edition-specific configuration
# Edition is resolved in version.mk (Professional, Academy, Casual)
ifeq ($(SCANDIUM_BUILD),true)
$(call inherit-product-if-exists, vendor/scandium/config/editions/$(SCANDIUM_EDITION_LOWER).mk)
endif

# Include ScandiumUI LatinIME dictionaries
PRODUCT_PACKAGE_OVERLAYS += vendor/scandium/overlay/dictionaries
PRODUCT_ENFORCE_RRO_EXCLUDED_OVERLAYS += vendor/scandium/overlay/dictionaries
