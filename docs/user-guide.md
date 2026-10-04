# Build and user instructions

1. Extract the starter package. Open `firmware/IBAC_Final_v1_1/IBAC_Final_v1_1.ino` in Arduino IDE.
2. Install/select the ESP8266 Community board package, LOLIN(WEMOS) D1 R2 & mini, USB COM port and a 4 MB flash layout with filesystem space. The prior manual named IDE 2.3.10 and core 3.1.2; compilation has not been repeated for this review package.
3. Keep the amplifier PTT collector disconnected during upload and initial checks. Verify, then upload via USB. Serial Monitor is 115200 baud. No separate web filesystem image is required: HTML is inside the sketch.
4. Join `IBAC-xxxx`, password `IBAC7051`; open http://192.168.4.1. Optional home Wi-Fi is configured under Settings and requires a restart. Fallback AP remains available. Use a trusted local network: the saved firmware has no web authentication and stores Wi-Fi credentials in LittleFS.
5. Set up Bluetooth using bluetooth.md. Confirm CI-V ACTIVE, expected frequency/band and Radio PTT/SEND RX.
6. Complete hardware-verification.md, including voltage and switching measurements, before connecting amplifier PTT or applying RF.

## Normal use

Power controller, radio and amplifier; allow Bluetooth to connect. Confirm the XPA's own displayed band matches the radio. Change bands only in RX and wait for voltage/amp switching to settle. Do not transmit on a mismatched band. Radio PTT/SEND is physical D7 state; Amplifier PTT is the software command, not feedback from the amplifier. Band target is a requested value, not a meter reading.

Unsupported frequencies (including 2 m/70 cm) inhibit XPA PTT. Source frequency ranges are classification ranges, not permission to transmit; observe your licence and local band limits. On shutdown stop transmitting, confirm RX, and follow radio/amplifier normal shutdown procedures.

## Troubleshooting

- No CI-V: check normal UART 9600, D5/D6 wiring, Bluetooth connection, address A4 and serialport function.
- No TX indication: check SEND output enabled for HF/50 MHz, cable continuity, D7 pull-up and RX/TX voltage.
- TX indication but no amp keying: check D1/transistor E/B/C and continuity to ACC 2.
- Wrong amp band: measure loaded band node at ACC 3, capacitor polarity and common ground; compare CSV defaults.
- Unexpected keying: disconnect collector and inspect the PTT circuit before reconnecting.

Change calibration/settings only in RX with amplifier PTT disconnected; restart and recheck loaded outputs afterward. The recovered firmware does not guarantee immediate reapplication of a saved voltage for the same band.
