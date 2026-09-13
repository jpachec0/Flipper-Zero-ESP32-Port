#include "internal_ext_fallback.h"

#include <string.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <wear_levelling.h>

#define EXT_VFS_PATH              "/sdcard"
#define INTERNAL_EXT_PARTITION    "ext_fat"
#define INTERNAL_EXT_MAX_FILES    12
#define INTERNAL_EXT_ALLOC_UNIT   4096

static const char* TAG = "InternalExt";
static wl_handle_t internal_wl = WL_INVALID_HANDLE;
static bool internal_mounted = false;

static bool internal_ext_mount_flash(void) {
    if(internal_mounted) return true;

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = INTERNAL_EXT_MAX_FILES,
        .allocation_unit_size = INTERNAL_EXT_ALLOC_UNIT,
    };

    ESP_LOGW(
        TAG,
        "External SD unavailable; mounting FATFS flash partition '%s' at %s",
        INTERNAL_EXT_PARTITION,
        EXT_VFS_PATH);

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        EXT_VFS_PATH, INTERNAL_EXT_PARTITION, &mount_config, &internal_wl);
    if(err != ESP_OK) {
        internal_wl = WL_INVALID_HANDLE;
        ESP_LOGE(TAG, "Internal /ext fallback mount failed: %s", esp_err_to_name(err));
        return false;
    }

    internal_mounted = true;
    ESP_LOGI(TAG, "Internal FATFS fallback mounted at %s", EXT_VFS_PATH);
    return true;
}

bool internal_ext_fallback_mount(void) {
    if(furi_hal_sd_is_mounted()) return true;
    if(internal_mounted) return true;

    if(furi_hal_sd_mount()) {
        ESP_LOGI(TAG, "Using external SD for /ext");
        return true;
    }

    return internal_ext_mount_flash();
}

bool internal_ext_fallback_unmount(void) {
    if(internal_mounted) {
        esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl(EXT_VFS_PATH, internal_wl);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "Internal /ext fallback unmount failed: %s", esp_err_to_name(err));
            return false;
        }
        internal_wl = WL_INVALID_HANDLE;
        internal_mounted = false;
        return true;
    }

    return furi_hal_sd_unmount();
}

bool internal_ext_fallback_is_mounted(void) {
    return internal_mounted || furi_hal_sd_is_mounted();
}

bool internal_ext_fallback_is_internal(void) {
    return internal_mounted;
}

FuriStatus internal_ext_fallback_info(FuriHalSdInfo* info) {
    if(!info) return FuriStatusError;

    if(!internal_mounted) {
        return furi_hal_sd_info(info);
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    esp_err_t err = esp_vfs_fat_info(EXT_VFS_PATH, &total_bytes, &free_bytes);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to query internal /ext FATFS: %s", esp_err_to_name(err));
        return FuriStatusError;
    }

    memset(info, 0, sizeof(*info));
    info->capacity = total_bytes;
    info->sector_size = 512;
    memcpy(info->product_name, "FLASH", 5);
    info->oem_id[0] = 'E';
    info->oem_id[1] = 'S';
    info->oem_id[2] = '\0';
    return FuriStatusOk;
}
