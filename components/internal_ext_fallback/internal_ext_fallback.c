#include "internal_ext_fallback.h"

#include <stdlib.h>
#include <string.h>

#include <diskio_impl.h>
#include <diskio_wl.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <ff.h>
#include <wear_levelling.h>

#define INTERNAL_EXT_PARTITION "ext_fat"
#define INTERNAL_EXT_PDRV 0
#define INTERNAL_EXT_DRIVE "0:"
#define INTERNAL_EXT_WORKBUF_SIZE 4096

static const char* TAG = "InternalExt";
static wl_handle_t internal_wl = WL_INVALID_HANDLE;
static bool internal_active = false;

static void internal_ext_release_flash(void) {
    if(internal_wl == WL_INVALID_HANDLE) return;

    (void)f_mount(NULL, INTERNAL_EXT_DRIVE, 0);
    ff_diskio_unregister(INTERNAL_EXT_PDRV);
    ff_diskio_clear_pdrv_wl(internal_wl);
    (void)wl_unmount(internal_wl);
    internal_wl = WL_INVALID_HANDLE;
    internal_active = false;
}

static bool internal_ext_prepare_filesystem(void) {
    FATFS probe_fs;
    FRESULT result = f_mount(&probe_fs, INTERNAL_EXT_DRIVE, 1);

    if(result == FR_NO_FILESYSTEM || result == FR_INT_ERR) {
        ESP_LOGW(TAG, "Internal /ext has no FAT filesystem; formatting once");

        void* workbuf = malloc(INTERNAL_EXT_WORKBUF_SIZE);
        if(!workbuf) {
            ESP_LOGE(TAG, "Unable to allocate FAT format work buffer");
            return false;
        }

        const MKFS_PARM options = {
            .fmt = (BYTE)(FM_ANY | FM_SFD),
            .n_fat = 0,
            .align = 0,
            .n_root = 0,
            .au_size = (UINT)wl_sector_size(internal_wl),
        };

        result = f_mkfs(INTERNAL_EXT_DRIVE, &options, workbuf, INTERNAL_EXT_WORKBUF_SIZE);
        free(workbuf);

        if(result != FR_OK) {
            ESP_LOGE(TAG, "Formatting internal /ext failed: %d", result);
            return false;
        }

        result = f_mount(&probe_fs, INTERNAL_EXT_DRIVE, 1);
        if(result != FR_OK) {
            ESP_LOGE(TAG, "Mount after internal /ext format failed: %d", result);
            return false;
        }

        (void)f_setlabel("Flipper INT");
    }

    if(result != FR_OK) {
        ESP_LOGE(TAG, "Internal /ext FAT probe failed: %d", result);
        return false;
    }

    result = f_mount(NULL, INTERNAL_EXT_DRIVE, 0);
    if(result != FR_OK) {
        ESP_LOGE(TAG, "Internal /ext FAT probe unmount failed: %d", result);
        return false;
    }

    return true;
}

static bool internal_ext_activate_flash(void) {
    if(internal_active) return true;

    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, INTERNAL_EXT_PARTITION);
    if(!partition) {
        ESP_LOGE(TAG, "Partition '%s' not found", INTERNAL_EXT_PARTITION);
        return false;
    }

    ESP_LOGW(
        TAG,
        "Physical SD unavailable; using internal FATFS '%s' (%lu bytes) for /ext",
        INTERNAL_EXT_PARTITION,
        (unsigned long)partition->size);

    esp_err_t err = wl_mount(partition, &internal_wl);
    if(err != ESP_OK) {
        internal_wl = WL_INVALID_HANDLE;
        ESP_LOGE(TAG, "Wear levelling mount failed: %s", esp_err_to_name(err));
        return false;
    }

    err = ff_diskio_register_wl_partition(INTERNAL_EXT_PDRV, internal_wl);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "FatFs diskio registration failed: %s", esp_err_to_name(err));
        internal_ext_release_flash();
        return false;
    }

    if(!internal_ext_prepare_filesystem()) {
        internal_ext_release_flash();
        return false;
    }

    internal_active = true;
    ESP_LOGI(
        TAG,
        "Internal /ext ready: usable=%lu sector=%lu",
        (unsigned long)wl_size(internal_wl),
        (unsigned long)wl_sector_size(internal_wl));
    return true;
}

FuriStatus internal_ext_fallback_init(bool power_reset) {
    if(internal_active) return FuriStatusOk;

    if(furi_hal_sd_init(power_reset) == FuriStatusOk) {
        ESP_LOGI(TAG, "Using physical microSD for /ext");
        return FuriStatusOk;
    }

    return internal_ext_activate_flash() ? FuriStatusOk : FuriStatusError;
}

bool internal_ext_fallback_is_present(void) {
    if(internal_active) return true;
    if(furi_hal_sd_is_present()) return true;

    return esp_partition_find_first(
               ESP_PARTITION_TYPE_DATA,
               ESP_PARTITION_SUBTYPE_DATA_FAT,
               INTERNAL_EXT_PARTITION) != NULL;
}

FuriStatus internal_ext_fallback_info(FuriHalSdInfo* info) {
    if(!info) return FuriStatusError;

    if(!internal_active) {
        return furi_hal_sd_info(info);
    }

    memset(info, 0, sizeof(*info));
    info->capacity = (uint64_t)wl_size(internal_wl);
    info->sector_size = (uint16_t)wl_sector_size(internal_wl);
    memcpy(info->product_name, "FLASH", 5);
    info->oem_id[0] = 'E';
    info->oem_id[1] = 'S';
    info->oem_id[2] = '\0';
    return FuriStatusOk;
}

bool internal_ext_fallback_is_internal(void) {
    return internal_active;
}
