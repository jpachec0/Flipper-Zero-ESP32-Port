idf_component_register(
    SRCS "internal_ext_fallback.c"
    INCLUDE_DIRS "."
    REQUIRES furi furi_hal fatfs wear_levelling esp_partition
)
