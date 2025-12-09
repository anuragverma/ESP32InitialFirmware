# ESP32InitialFirmware
Initial firmware for ESP32 dev boards and WT32-ETH01. Boots as a Wi-Fi AP with a captive-style portal to toggle the onboard LED and upload a new firmware image (with confirmation) before rebooting.

## Building
- Install [PlatformIO](https://platformio.org/).
- From this directory run one of:
  - `pio run -e esp32dev`
  - `pio run -e wt32-eth01`
- To flash over serial: add `-t upload` (e.g., `pio run -e esp32dev -t upload`).
- Firmware version is auto-derived from the build environment plus git tag/describe (or UTC timestamp when no tag is present).

## Features
- Starts AP `FundebaziEsp32` (esp32dev) or `FundebaziWt32` (WT32-ETH01) with password `fundebazi`, captive-style redirect to UI at `http://192.168.4.1/`.
- Single-page UI to toggle the LED, upload firmware, view version.
- OTA upload with confirmation step; clears Wi-Fi/config before rebooting into the new image.
- Optional mDNS responder `fundebazi.local`.

## Manual test checklist
- Build and flash: `pio run -e esp32dev -t upload` or `pio run -e wt32-eth01 -t upload`.
- Power cycle: AP appears with the environment’s SSID; connect using password above.
- Opening any site redirects to UI at `http://192.168.4.1/`.
- Toggle button changes the LED and updates UI state.
- Upload a valid `.bin`; receive “Upload complete. Confirm to apply.”
- Click “Apply & Reboot”; device erases config, reboots, and new firmware runs.
- `/info` endpoint returns JSON with version and LED state.
