# GMS
ifeq ($(WITH_GMS),true)
WITH_GMS_COMMS_SUITE := true
endif

PRODUCT_PACKAGES += \
    sensitive_pn.xml

PRODUCT_PACKAGES += \
    apns-conf.xml

PRODUCT_PACKAGES += \
    messaging \
    Stk

ifeq ($(SCANDIUM_BUILD),true)
PRODUCT_PRODUCT_PROPERTIES += \
    ro.config.ringtone=Orion.ogg
endif

PRODUCT_PRODUCT_PROPERTIES += \
    net.tethering.noprovisioning=true

PRODUCT_PRODUCT_PROPERTIES += \
    ro.com.android.mobiledata=false
