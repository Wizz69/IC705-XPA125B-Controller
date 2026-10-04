# IC705-XPA125B-Controller

IBAC: Icom Bluetooth Amplifier Controller for IC-705, HC-05 Bluetooth Classic SPP, Wemos D1 Mini ESP8266 and Xiegu XPA125B (HF/6 m).

## Review package — 3 October 2026

Contains the recovered 12 September 2026 `IBAC_Final_v1_1.ino`, unchanged byte-for-byte. Its runtime version is 1.1.0. The opening comment still says V1.0.0 and 10 kΩ; the actual pin comment and latest project manual specify a 1 kΩ band-filter resistor. This historical inconsistency is documented rather than silently editing your software.

This is a repository starter for review, not a newly bench-tested release. No licence grant is inferred; choose a licence before inviting reuse.

## Files

- `firmware/IBAC_Final_v1_1/IBAC_Final_v1_1.ino`: complete recovered Arduino sketch, including CI-V parser, PWM band output, SEND/PTT logic, Wi-Fi status/settings/engineer pages and LittleFS configuration.
- `docs/wiring.md`: point-to-point connections and connector pinout.
- `docs/band-voltages.csv`: complete source-default table.
- `docs/bluetooth.md`: pairing and AT-mode notes.
- `docs/user-guide.md`: build, upload and operation.
- `docs/hardware-verification.md`: reported results, unresolved conflicts and commissioning record.
- `docs/source-review.md`: known software limitations and validation status.

## Arduino layout

Open the sketch inside the identically named folder. Use Arduino IDE with ESP8266 Community core; the previous manual names 3.1.2. Select LOLIN(WEMOS) D1 R2 & mini, with a flash layout providing LittleFS. Headers used: Arduino, ESP8266WiFi, ESP8266WebServer, ESP8266mDNS, LittleFS and SoftwareSerial (provided with the ESP8266 package). A generic ESP32 or AVR target will not work.

Read the hardware-verification checklist before connecting amplifier PTT. The web display reports commanded outputs; it does not measure amplifier state or actual band voltage.
