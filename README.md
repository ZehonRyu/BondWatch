# BondWatch

Companion smartwatch: ESP32-S3 firmware on a 240×320 ST7789 face, plus a phone/PC companion later. The watch itself is control, settings, and presence — flashlight and alarm stay on the phone.

Hardware target: **Goouuu ESP32-S3-N16R8** + **Waveshare 2.0" ST7789** (240×320) + **CST816** touch + **INMP441** mic.

## Documents

Open these in a browser (they are static HTML, no server required):

| File | What it is |
|------|------------|
| [docs/bondwatch-board.html](docs/bondwatch-board.html) | Project board, Gantt, buy list |
| [docs/bondwatch-wiring.html](docs/bondwatch-wiring.html) | Pin map and physical wiring |
| [docs/bondwatch-software-design.html](docs/bondwatch-software-design.html) | Software / UI design |
| [docs/bondwatch-ai-engineer.html](docs/bondwatch-ai-engineer.html) | Cloud AI engineer notes |
| [docs/bondwatch-sim.html](docs/bondwatch-sim.html) | On-device UI mock (portrait + landscape) |

Wokwi diagram: `diagram.json` + `wokwi.toml`. Pin source of truth: `firmware/include/pins.h`.

## Repo layout

```
firmware/     ESP32-S3 Arduino / PlatformIO firmware
docs/         Offline HTML docs + UI sim
cloud/        Companion API + 3D watch sim (Docker)
app/          Flutter companion (early)
tools/        Air780E AT loopback helpers (no serial-tool binaries)
scripts/      Screen capture / APK helpers
```

## Watch UI

Portrait **240×320** and landscape **320×240** are both first-class.

- Home: large clock + character (full body in portrait, big face in landscape)
- Swipe **up** → Control (DND / brightness / volume)
- Swipe **left** → Settings (language / orientation / touch calibration)
- Swipe **down** → back
- Tap on home → acknowledge (voice is off while `VOICE_FEATURES` is 0)

Touch calibration is a two-point wizard that picks the best X/Y flip map and can self-correct a 180° mismatch.

## Firmware

Needs [PlatformIO](https://platformio.org/).

```bash
cp firmware/include/secrets.example.h firmware/include/secrets.h
# edit Wi-Fi / API if you are not using OFFLINE_USB
python -m platformio run
python -m platformio run -t upload --upload-port COMx
```

Do **not** flash:

- **COM1** — motherboard UART (`ACPI\PNP0501`), not the ESP32
- The CH340 that is wired to the **Air780E AT** board

ESP32 download on this desk has been a **CH340** (`USB VID:PID=1A86:7523`). If upload says “No serial data received”, hold **BOOT**, tap **RESET**, then upload again.

## Air780E AT (4G module, separate from the watch)

Verified wiring to CH340 when testing AT:

| CH340 | Air780E board |
|-------|----------------|
| 5V | 5V |
| GND | G |
| TXD | TX (same-name, do not cross) |
| RXD | RX (same-name, do not cross) |
| GND (or on-board PK↔G) | PK |

Leave DP/DM/VB disconnected for AT. CH340 jumper: **3.3V**. Script: `tools/sscom/test_air780e_loopback.py`.

## Cloud companion

```bash
cp .env.example .env   # add DEEPSEEK_API_KEY if you use the LLM
docker compose up --build
```

- API: port **11111**
- 3D watch sim: port **11112**

## Secrets

`firmware/include/secrets.h` and `.env` are gitignored. Start from the `*.example` files. Never commit Wi-Fi passwords or API keys.
