# Source provenance and review

Recovered file: `IBAC_Final_v1_1.ino`, saved 12 September 2026, 22,826 bytes. The review package retains its exact bytes in the Arduino-compatible folder. Runtime FW_VERSION is 1.1.0; header wording V1.0.0 and 10 kΩ is stale. Current code pin comment says 1 kΩ, matching the latest manual.

## Validation performed

Source inspected for pins, default voltages, dependencies, Arduino folder naming and documented control behaviour. SHA-256 identity checked against recovered source; ZIP integrity checked. No Arduino compiler is available in this workspace, so no new compile, flashing or hardware test is claimed.

## Known limitations requiring a future firmware revision

- Freshness is shared by frequency and mode replies. Continuing mode replies can keep an old frequency “fresh”; frequency needs its own timeout for a stronger inhibit.
- No explicit RC/amp band-settling delay before keying. Applied band index represents a commanded output, not settled voltage or amplifier feedback.
- Settings can be saved during TX; outputs are not immediately reapplied for a changed target on the same band. Change settings only with PTT disconnected and restart/recommission.
- Firmware holds PTT 15 ms after SEND rises. Actual amp timing requires measurement.
- HTTP handlers, Wi-Fi and filesystem operations share the main loop with PTT service; worst-case timing is unmeasured.
- Web settings have no authentication; Wi-Fi password is stored in plaintext and returned in the settings form. Use only trusted local access.
- Startup and reset fail-safe behaviour depends on actual transistor/pull-down wiring, not just source logic.

No fixes are silently introduced here: this delivers the previously written software for review. A ready-to-publish source package is not a verified safe-to-flash release. Do not describe these limitations as resolved until the software is revised and tested.
