# LilyGo T-Embed CC1101 — internal `/ext` fallback test

Branch: `feature/internal-ext-fallback`

This branch replaces the T-Embed dual-OTA layout with a single 4.5 MiB factory app partition and an approximately 11.31 MiB raw FAT partition (`ext_fat`) used as `/ext` whenever the external microSD cannot be mounted.

## Important behavior

- External microSD still has priority.
- With no mountable microSD, `ext_fat` is mounted at the same VFS path used by storage (`/sdcard`), so Flipper `/ext/...` paths continue to work.
- The internal FAT volume is formatted automatically on its first successful fallback mount.
- OTA firmware update is intentionally unavailable on this layout because there is no second OTA application slot.
- USB ROM download/recovery remains available. This branch does not touch eFuses, Secure Boot, or flash encryption.

## Build

ESP-IDF **v5.4.1** is required.

```bash
git checkout feature/internal-ext-fallback
git pull
chmod +x tools/internal_ext_build_package.sh
./tools/internal_ext_build_package.sh
```

The helper refuses to package a firmware image that does not fit the 4.5 MiB application partition.

## First flash

Because this changes the partition table, do the first test as a clean/full flash using the ESP32-S3 USB download connection that is known to work on the board.

After the clean erase, either use the generated package or build/flash directly:

```bash
./buildAndFlash_T-Embed.sh --port /dev/ttyACM0 --monitor
```

Replace `/dev/ttyACM0` with the actual device path.

On first boot, leave the microSD removed. Expected behavior is:

1. external SD initialization/mount fails;
2. `InternalExt` mounts (and, on first use, formats) `ext_fat`;
3. storage starts with `/ext` available.

If the device no longer boots, enter ESP32-S3 ROM download mode, erase the flash, and flash a known-good Bruce/full image with its own partition table.

## Prepare the official Sor3nt SD assets

Official current package:

`https://sor3nt.github.io/release/t-embed/latest/sdcard.zip`

Download and extract it on the computer. Upload the **contents** of the extracted directory, not the ZIP itself, to `/ext` after the internal fallback has mounted. qT-Embed's storage/file-management path is preferred because it writes through the firmware storage API; do not write a plain FAT image directly over `ext_fat`, because this partition is mounted through ESP-IDF wear levelling.

Linux example for preparing the files locally:

```bash
curl -fL https://sor3nt.github.io/release/t-embed/latest/sdcard.zip -o sdcard.zip
rm -rf internal_ext_assets
mkdir internal_ext_assets
unzip sdcard.zip -d internal_ext_assets
du -sh internal_ext_assets
```

The raw `ext_fat` partition is `0xB50000` bytes (~11.31 MiB); usable FAT capacity is slightly lower because wear-levelling/filesystem metadata consumes space. If the extracted starter pack is larger than the available volume, upload the core tree first (`Manifest`, `dolphin`, `apps_assets`, `apps_data`, `nfc`, `subghz`, `infrared`, `badusb`, etc.) and leave optional large media/content out.

## Serial verification

The key log lines to look for are similar to:

```text
External SD unavailable; mounting FATFS flash partition 'ext_fat' at /sdcard
Internal FATFS fallback mounted at /sdcard
Storage service started
```

After that, test several normal `/ext` operations from the UI: open Archive, create a small file/folder if supported, reboot, and confirm it persists.
