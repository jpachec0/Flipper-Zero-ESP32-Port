# LilyGo T-Embed CC1101 — internal `/ext` fallback validation

Branch: `feature/internal-ext-fallback`

User-facing setup is documented in [`INTERNAL_EXT.md`](INTERNAL_EXT.md). This file keeps the implementation/build/runtime checks used while validating the feature on hardware.

## Layout and backend

The T-Embed 16 MB layout is intentionally single-app:

```text
nvs       0x009000    24 KiB
phy_init  0x00F000     4 KiB
factory   0x010000  4608 KiB
ext_fat   0x490000 11584 KiB
coredump  0xFE0000   128 KiB
```

Important implementation detail: Flipper storage uses FatFs drive `0:` directly. The fallback therefore does **not** mount a POSIX `/sdcard` VFS. When physical SD initialization fails it:

1. finds the `ext_fat` data/FAT partition;
2. mounts ESP-IDF wear levelling with `wl_mount()`;
3. registers that wear-levelled device as FatFs physical drive 0 with `ff_diskio_register_wl_partition()`;
4. formats it once on first use if no FAT filesystem exists;
5. hands drive `0:` back to the normal storage service, which mounts its own `FATFS` object and exposes the usual Flipper `/ext/...` namespace.

Physical microSD still has priority.

## Build

ESP-IDF **v5.4.1** is required.

```bash
git checkout feature/internal-ext-fallback
git pull --ff-only
source ~/esp/esp-idf/export.sh
bash build.sh --board t_embed --build-only
```

Expected build properties:

- board: LilyGO T-Embed CC1101;
- app partition: `0x480000` (4.5 MiB);
- `ext_fat`: `0xB50000` bytes raw;
- firmware image must remain below the app-partition limit.

Decode the actual generated partition table when changing layout code:

```bash
python ~/esp/esp-idf/components/partition_table/gen_esp32part.py \
  build_t_embed/partition_table/partition-table.bin
```

## First flash

Because the partition table changes, the first test must be a full/clean flash. With the board on a reliable USB port:

```bash
idf.py -B build_t_embed -p /dev/ttyACM0 erase-flash
idf.py -B build_t_embed -p /dev/ttyACM0 flash
idf.py -B build_t_embed -p /dev/ttyACM0 monitor
```

Replace the port as needed.

Leave the microSD removed for the fallback test.

## Validated runtime sequence

A successful boot without physical SD should contain the same sequence we observed on hardware:

```text
FuriHalSd: SD init failed: ESP_ERR_TIMEOUT
InternalExt: Physical SD unavailable; using internal FATFS 'ext_fat' (...) for /ext
InternalExt: Internal /ext ready: usable=11755520 sector=4096
StorageExt: card mounted
```

The storage service needs additional stack for the wear-levelling/FatFs first-mount path. `StorageSrv` is therefore configured with 8 KiB stack, and the temporary FatFs probe object is heap allocated.

## User provisioning regression checks

With internal fallback active and `/ext` initially empty:

1. Open **WiFi**. The menu should show **Setup Internal Storage**.
2. Run it while connected to WiFi. The firmware must skip the OTA firmware phase and start the SD-file delta sync directly.
3. The internal profile must preflight free space before writing.
4. Nested individual IRDB packs below `infrared/assets/<category>/...`, bundled Doom WAD and bundled sample media must not be selected for internal flash.
5. If the first profile does not leave the configured reserve, the compact profile may additionally omit `infrared/assets/leds.ir`.
6. Core files including `/ext/Manifest` and `/ext/infrared/assets/tv.ir` must be present after a successful setup.
7. Re-running **Sync Internal Storage** must repair a deleted included file even when `/ext/version.txt` already matches the remote version.
8. A successful internal setup writes `/ext/.internal_profile`.

## Web-Filesystem regression checks

With `/ext/webfs/index.html` absent:

1. Start Web-Filesystem in STA or dedicated-AP mode.
2. Opening `/` must serve the firmware-embedded bootstrap uploader, not an instruction telling the user to copy `index.html` manually.
3. `/api/info` must report total/free bytes and `internal: true` for the fallback backend.
4. Folder upload must strip the selected top-level local folder and place its contents directly under `/ext`.
5. Uploading a selection larger than the reported internal free space must be refused by the bootstrap UI before transfer.
6. A single `/api/upload` larger than available free space plus the existing destination file's reclaimable size must be rejected before truncation.
7. If `/ext/webfs/index.html` is later installed, it must take precedence over the embedded bootstrap UI.

## Limitations/recovery

- Wireless OTA is intentionally unavailable on this partition layout because no inactive OTA application slot exists.
- Raw USB Mass Storage remains physical-microSD-only. Do not expose `ext_fat` as raw sectors; its on-flash representation is managed by ESP-IDF wear levelling.
- The feature does not change eFuses, Secure Boot or flash encryption.
- If a development build does not boot, use ESP32-S3 ROM download mode and full-flash a known-good image with its matching partition table.
