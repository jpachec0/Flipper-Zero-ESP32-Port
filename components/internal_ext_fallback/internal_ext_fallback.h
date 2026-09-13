#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi_hal_sd.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Mount external SD when available, otherwise mount the internal FATFS
 * partition at the same VFS path used by storage (/sdcard).
 */
bool internal_ext_fallback_mount(void);

/** Unmount whichever backend currently serves /ext. */
bool internal_ext_fallback_unmount(void);

/** Return true when either SD or the internal FATFS fallback is mounted. */
bool internal_ext_fallback_is_mounted(void);

/** Return true when /ext is currently backed by internal flash. */
bool internal_ext_fallback_is_internal(void);

/** Fill SD-compatible geometry/capacity information for the active backend. */
FuriStatus internal_ext_fallback_info(FuriHalSdInfo* info);

#ifdef __cplusplus
}
#endif
