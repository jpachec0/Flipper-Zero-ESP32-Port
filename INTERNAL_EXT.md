# Internal `/ext` on LilyGO T-Embed CC1101

This branch can use part of the T-Embed's 16 MB SPI flash as Flipper `/ext` when no usable microSD is present.

## What users get

- A physical microSD still has priority whenever it initializes successfully.
- Without a usable microSD, the firmware mounts the `ext_fat` partition through ESP-IDF wear levelling and registers it as FatFs drive `0:`. Flipper storage therefore continues to use normal `/ext/...` paths.
- The internal FAT volume is formatted automatically the first time it is needed.
- The usable internal `/ext` capacity is about 11.2 MiB, so the complete official `sdcard.zip` does **not** fit.
- The firmware includes a capacity-aware internal starter profile that keeps the files required by the normal applications while omitting large optional content.
- Web-Filesystem has a bootstrap UI compiled into the firmware, so an empty `/ext` is still manageable from a browser.

## First flash

The internal-storage layout changes the partition table. The first installation must therefore be a full flash (bootloader + partition table + application), not an application-only update.

Build with ESP-IDF v5.4.1:

```bash
source ~/esp/esp-idf/export.sh
bash build.sh --board t_embed --build-only
```

Then flash the generated build. A clean erase is recommended when moving from the normal dual-OTA layout to this layout.

After flashing, boot once without the microSD inserted. Expected serial output contains lines similar to:

```text
InternalExt: Physical SD unavailable; using internal FATFS 'ext_fat' (...) for /ext
InternalExt: Internal /ext ready: usable=... sector=4096
StorageExt: card mounted
```

## Recommended setup — no PC file copying

1. Open **WiFi** on the T-Embed.
2. Select **Setup Internal Storage**.
3. If the device is not connected yet, choose your WiFi network and connect.
4. The device downloads the current official file manifest and installs the internal-flash profile automatically.
5. After a successful setup the menu item changes to **Sync Internal Storage**.

The internal profile intentionally skips large optional release content that is not required for the core applications:

- nested individual IRDB remote packs below `infrared/assets/<category>/...`;
- the bundled `doom1.wad`;
- bundled example media under `apps_data/medien/`;
- `infrared/assets/leds.ir` only when the normal internal profile still cannot leave a safe free-space reserve.

The consolidated Universal Remote databases such as `infrared/assets/tv.ir`, `ac.ir`, `audio.ir`, `fans.ir` and `projectors.ir` remain eligible for the internal profile. `Manifest` is also installed, so the desktop does not remain in the "No databases" state.

Before downloading, the updater estimates FAT cluster usage and keeps a free-space reserve. If the selected profile cannot fit, setup stops **before writing files** instead of filling the filesystem halfway through the transfer.

Internal storage always verifies the remote manifest even when `version.txt` already matches. This repairs missing files after an interrupted copy instead of incorrectly declaring a partial filesystem up to date.

## Web-Filesystem bootstrap

Web-Filesystem no longer depends on `/ext/webfs/index.html` for first use.

If that file is missing, the firmware serves a built-in browser uploader that can:

- show whether `/ext` is internal flash or physical SD;
- display total/free space;
- upload an extracted folder directly into `/ext`;
- upload individual files;
- create parent directories automatically;
- reject a selection that obviously cannot fit before starting;
- reject a single HTTP upload before truncating the existing file if there is not enough free space.

If `/ext/webfs/index.html` is later installed, the normal full Web-Filesystem UI takes precedence automatically.

## Important limitations

### No wireless OTA on this layout

The internal `/ext` partition uses the flash space that would otherwise be needed for a second OTA application slot. Firmware OTA is therefore intentionally skipped when the internal fallback is active.

Use USB full flash for firmware upgrades that change the application image or partition table.

### USB Mass Storage remains physical-SD-only

The fallback is injected into the Flipper storage service, not the raw SD-sector HAL. This is deliberate: the internal partition uses ESP-IDF wear levelling and must not be exposed as if it were a raw ordinary SD card.

Use the normal Storage/RPC path, Web-Filesystem or the built-in internal-storage synchronizer to manage internal `/ext` files.

## Recovery

This feature does not modify eFuses, Secure Boot or flash encryption. If a development build does not boot, use the ESP32-S3 ROM download mode and perform a full flash with a known-good image and matching partition table.
