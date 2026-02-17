$(call inherit-product, vendor/scandium/config/common.mk)

ifeq ($(SCANDIUM_BUILD),true)
$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackage14.mk)
include vendor/scandium/config/aosp_audio.mk

include vendor/scandium/config/scandium_audio.mk
PRODUCT_PRODUCT_PROPERTIES += \
    ro.config.notification_sound=Argon.ogg \
    ro.config.alarm_alert=Hassium.ogg
endif

# Apps — edition-specific apps (Backgrounds, Glimpse, etc)
# config/editions/{professional,academy,casual}.mk
PRODUCT_PACKAGES += \
    AvatarPicker \
    LatinIME

# Launcher is now managed by edition configs
# See config/editions/{professional,academy,casual}.mk
PRODUCT_PACKAGES += \
    Launcher3Overlay

PRODUCT_PACKAGES += \
    charger_res_images

ifneq ($(WITH_SCANDIUM_CHARGER),false)
PRODUCT_PACKAGES += \
    scandium_charger_animation \
    scandium_charger_animation_vendor
endif

PRODUCT_PRODUCT_PROPERTIES += \
    ro.scandium.legal.url=https://scandiumui.tech/legal

PRODUCT_PRODUCT_PROPERTIES += \
    media.recorder.show_manufacturer_and_model=true

PRODUCT_PACKAGES += \
    QuickAccessWallet

PRODUCT_PACKAGES += \
    libtextclassifier_annotator_en_model \
    libtextclassifier_annotator_universal_model \
    libtextclassifier_actions_suggestions_universal_model \
    libtextclassifier_lang_id_model

PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += \
    system/etc/textclassifier/actions_suggestions.universal.model \
    system/etc/textclassifier/lang_id.model \
    system/etc/textclassifier/textclassifier.en.model \
    system/etc/textclassifier/textclassifier.universal.model

# Themes are now managed by edition configs
# See config/editions/{professional,academy,casual}.mk
