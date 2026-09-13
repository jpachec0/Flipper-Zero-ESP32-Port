idf_component_register(
    INCLUDE_DIRS "."
    REQUIRES furi furi_hal toolbox internal_ext_fallback
)

# Keep the storage service API unchanged while routing its SD-facing mount,
# unmount and info calls through the /ext backend selector. The selector still
# prefers the real SD card and only falls back to internal FATFS when needed.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    furi_hal_sd_mount=internal_ext_fallback_mount
    furi_hal_sd_unmount=internal_ext_fallback_unmount
    furi_hal_sd_info=internal_ext_fallback_info
)
