# Board reference

Every board the library supports, with notes for the boards that exist in several revisions. The [main README](../README.md) shows which boards ship with a Clawdmeter config.

There is one hardware file per display in `hardware/`. Include the one that matches your device:

```yaml
packages:
  hardware: !include esphome-modular-lvgl-buttons/hardware/<model>.yaml
```

How to read the tables:

- Model is the file name without `.yaml`.
- Resolution is the native panel size. Some files rotate the view by default.
- Driver is the ESPHome display platform the file uses.
- Every board file needs ESPHome 2026.7.0 or newer. `guition-esp32-p4-jc8012p4a1-v2` and `-v3` need 2026.8.0.

## Symptoms of a wrong hardware file

| Symptom | Board | Cause |
|---|---|---|
| Blank or garbled screen | Guition JC8012P4A1 | Wrong revision file. See [JC8012P4A1 revisions](#jc8012p4a1-revisions). |
| Display works, but the board reboots at boot. Or ESPHome rejects `engineering_sample`. | Guition JC8012P4A1 | Wrong one of `-v2` and `-v3` |
| White screen with a vertical noise band | Guition JC1060P470 | File and panel revision do not match. See [JC1060P470 V2 panel](#jc1060p470-v2-panel). |
| Blank panel, device ends in safe mode | Waveshare 10.1-DSI-TOUCH-A | The panel enable sequence did not run. See [Two-piece DSI kits](#two-piece-dsi-kits). |

## Waveshare

| Model | Size | Resolution | Driver |
|---|---|---|---|
| `waveshare-esp32-s3-touch-lcd-2.8c` | 2.8" round | 480×480 | `st7701s` |
| `waveshare-esp32-s3-touch-lcd-3.5` | 3.5" | 320×480 | `mipi_spi` |
| `waveshare-esp32-s3-touch-lcd-3.5b` | 3.5" | 320×480 | `mipi_spi` |
| `waveshare-esp32-s3-touch-lcd-4` | 4.0" | 480×480 | `st7701s` |
| `waveshare-esp32-s3-touch-lcd-4-v4` | 4.0" | 480×480 | `mipi_rgb` |
| `waveshare-esp32-s3-touch-lcd-4.3` | 4.3" | 800×480 | `rpi_dpi_rgb` |
| `waveshare-esp32-s3-touch-lcd-7` | 7.0" | 800×480 | `mipi_rgb` |
| `waveshare-esp32-s3-touch-lcd-7B` | 7.0" | 1024×600 | `mipi_rgb` |
| `waveshare-esp32-p4-86-panel` | 86 mm | 720×720 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-touch-lcd-3.4c` | 3.4" | 800×800 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-touch-lcd-4b` | 4.0" | 720×720 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-touch-lcd-7` | 7.0" | 720×1280 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-touch-lcd-7b` | 7.0" | 1024×600 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-touch-lcd-10.1` | 10.1" | 800×1280 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-7-DSI-TOUCH-A` | 7.0" | 720×1280 | `mipi_dsi` |
| `waveshare-esp32-p4-wifi6-10.1-DSI-TOUCH-A` | 10.1" | 800×1280 | `mipi_dsi` |
| `waveshare-esp32-p4-module-dev-kit-10.1-DSI-TOUCH-A` | 10.1" | 800×1280 | `mipi_dsi` |

### Two-piece DSI kits

The three DSI-TOUCH-A files are two-piece combinations. Each is a dev board in Raspberry Pi form factor with a separate DSI panel on the MIPI-DSI FPC, not an all-in-one unit.

| Model | Dev board | Panel |
|---|---|---|
| `waveshare-esp32-p4-wifi6-7-DSI-TOUCH-A` | [ESP32-P4-WIFI6-DEV-KIT](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-DEV-KIT) | [7-DSI-TOUCH-A](https://www.waveshare.com/wiki/7-DSI-TOUCH-A): ILI9881C, 720×1280, 1Gbps |
| `waveshare-esp32-p4-wifi6-10.1-DSI-TOUCH-A` | the same WIFI6 dev kit | [10.1-DSI-TOUCH-A](https://www.waveshare.com/wiki/10.1-DSI-TOUCH-A): JD9365, 800×1280, 1.5Gbps |
| `waveshare-esp32-p4-module-dev-kit-10.1-DSI-TOUCH-A` | [ESP32-P4-Module-DEV-KIT](https://www.waveshare.com/wiki/ESP32-P4-Module-DEV-KIT) | the same 10.1 panel |

The Module dev kit is wired the same way as the WIFI6 dev kit, but ships 16MB flash instead of 32MB. Some batches of the Module ship 32MB. Its hardware file sets 16MB.

- Pass `rotation: "270"` for landscape mounting on either 10.1 profile.
- None of these boards routes an LCD reset GPIO or a backlight PWM GPIO. Panel brightness is written over I2C, device `0x45`, register `0x96`.
- The GT9271 touch controller is polled, because the DSI FPC carries no INT or RST line.

The 10.1 panel has no enable GPIO. It is switched on by a write to register `0x95` on I2C device `0x45`. ESPHome's `mipi_dsi` never sends that write. Both 10.1 profiles therefore replay the vendor's enable sequence at `on_boot` priority 900. Without the sequence, the boot wedges in the DSI command FIFO after about 19 commands and ends in safe mode with a blank panel. The comments in the two hardware files have the details. Stock ESPHome needs no patch for this.

## Guition

| Model | Size | Resolution | Driver |
|---|---|---|---|
| `guition-esp32-s3-4848s040` | 4.0" | 480×480 | `mipi_rgb` |
| `guition-esp32-jc4827w543` | 4.3" | 480×272 | `qspi_dbi` |
| `guition-esp32-jc8048w535` | 3.5" | 320×480 | `qspi_dbi` |
| `guition-esp32-jc8048w550` | 5.0" | 800×480 | `mipi_rgb` |
| `guition-esp32-p4-jc1060p470` | 7.0" | 1024×600 | `mipi_dsi` |
| `guition-esp32-p4-jc1060p470c` | 7.0" | 1024×600 | `mipi_dsi` |
| `guition-esp32-p4-jc1060p470-v2` | 7.0" | 1024×600 | `mipi_dsi` |
| `guition-esp32-p4-jc1060p470c-v2` | 7.0" | 1024×600 | `mipi_dsi` |
| `guition-esp32-p4-jc4880p443` | 4.3" | 480×800 | `mipi_dsi` |
| `guition-esp32-p4-jc8012p4a1` | 10.1" | 800×1280 | `mipi_dsi` |
| `guition-esp32-p4-jc8012p4a1-v2` | 10.1" | 800×1280 | `mipi_dsi` |
| `guition-esp32-p4-jc8012p4a1-v3` | 10.1" | 800×1280 | `mipi_dsi` |

`guition-esp32-jc8048w535` is the 3.5" board that Guition sells as JC3248W535. The file keeps the name it has upstream. Its display runs on `qspi_dbi`, and its touch controller is an `axs15231`.

### JC8012P4A1 revisions

The three `jc8012p4a1` files are the same 10.1" board at different revisions. **The wrong one leaves the screen blank or garbled.**

| Model | Use it for |
|---|---|
| `guition-esp32-p4-jc8012p4a1` | The original panel with the `JC8012P4A1` init sequence |
| `guition-esp32-p4-jc8012p4a1-v2` | Later units with a different LCD behind the same JD9365 driver, on pre-rev3 ESP32-P4 engineering-sample silicon |
| `guition-esp32-p4-jc8012p4a1-v3` | The same later LCD on production ESP32-P4 silicon, rev3 |

If the display works but the board reboots at boot, or ESPHome rejects `engineering_sample`, you are on the wrong one of `-v2` and `-v3`. Check the boot log for `chip revision: vX.Y`.

### JC1060P470 V2 panel

`guition-esp32-p4-jc1060p470-v2` is the 2026 "New Panel" revision of the 7" board. Guition prints "V2" in the SKU or material number on the label on the back of the case. Boards without that marking use `guition-esp32-p4-jc1060p470`.

**The wrong profile gives a white screen with a vertical noise band.**

The V2 panel keeps the same JD9165 driver. It differs in three points:

- It needs its own init sequence and timings.
- Reset is on GPIO0, not GPIO27.
- The ESP32-C6 SDIO clock is lowered to 10MHz, so that it does not interfere with the MIPI-DSI stream.

`guition-esp32-p4-jc1060p470c-v2` is the same panel revision on the `-c` board variant. That variant keeps its own LDO channel and audio pin map, and has no status LED.

## Sunton

| Model | Size | Resolution | Driver |
|---|---|---|---|
| `sunton-esp32-2432s028` | 2.8" | 320×240 | `mipi_spi` |
| `sunton-esp32-2432s028R` | 2.8" | 320×240 | `ili9xxx` |
| `sunton-esp32-4827s032R` | 3.2" | 480×280 | `rpi_dpi_rgb` |
| `sunton-esp32-8048s043` | 4.3" | 800×480 | `mipi_rgb` |
| `sunton-esp32-8048s050` | 5.0" | 800×480 | `rpi_dpi_rgb` |
| `sunton-esp32-8048s070` | 7.0" | 800×480 | `mipi_rgb` |

- `sunton-esp32-4827s032R` sets a panel height of 280. Its file comment says that 272 shows garbage.
- `sunton-esp32-8048s043` is for hardware v1.1 or later. The version is printed next to the SD card slot.

## Elecrow and others

| Model | Size | Resolution | Driver |
|---|---|---|---|
| `elecrow-esp32-7inch` | 7.0" | 800×480 | `rpi_dpi_rgb` |
| `crowpanel-adv-10.1inch-esp32p4-hmi` | 10.1" | 1024×600 | `mipi_dsi` |
| `crowpanel-adv-10.1inch-esp32p4-hmi_v1.2` | 10.1" | 1024×600 | `mipi_dsi` |
| `esp32-s3-box-3` | 2.4" | 320×240 | `ili9xxx` |
| `lilygo-tdisplays3` | 1.9" | 170×320 | `ili9xxx` |
| `SDL-lvgl` | desktop window | variable | `sdl` |

The two CrowPanel files are for the Elecrow CrowPanel Advanced 10.1" with an ESP32-P4 and an ESP32-C6 co-processor for Wi-Fi. Hardware version 1.2 talks to the co-processor over 4 SDIO lanes in a different pin order, so it has its own file.

## Helper files

Three more files in `hardware/` are not boards:

| File | Use |
|---|---|
| `generic.yaml` | Starting template for a board that is not listed |
| `media_player-voice_assistant.yaml` | Audio and voice add-on |
| `guition-esp32-jc4827w543-audio.yaml` | Audio add-on for the JC4827W543 |

Include an audio add-on alongside a screen file. It does not work standalone.
