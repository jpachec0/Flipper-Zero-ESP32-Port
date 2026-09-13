#pragma once

#include <stdbool.h>
#include <furi_hal_sd.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the physical SD card when possible. If SD initialization fails,
 * register the internal wear-levelled FAT partition as FatFs drive 0 instead.
 */
FuriStatus internal_ext_fallback_init(bool power_reset);

/** Storage-facing presence check. The internal FAT partition keeps /ext
 * logically present even when no physical microSD can be initialized.
 */
bool internal_ext_fallback_is_present(void);

/** Fill SD-compatible information for whichever backend currently serves /ext. */
FuriStatus internal_ext_fallback_info(FuriHalSdInfo* info);

/** True once this boot has switched /ext to the internal flash backend. */
bool internal_ext_fallback_is_internal(void);

#ifdef __cplusplus
}
#endif
