# Hardware verification record

## Evidence already available

- User previously reported SEND working and IBAC showing TX.
- Saved 12 September v1.1 manual records D1 PTT, D2 1 kΩ/10 µF filter, D7 SEND with 10 kΩ pull-up, TRS tip SEND and ACC 2 PTT/3 band/6 ground.
- Source confirms GPIO assignments and nominal band defaults.

These are prior reports and saved documentation, not new bench measurements. “All okay” does not certify every protection or failure mode. Earlier project notes differ on GPIOs, TRS tip/ring, ACC 2/3 and band targets; latest documentation is the review baseline, with conflicts retained here.

## Unverified before release / before a new build

- [ ] Record actual connector contact numbering/view and cable continuity; resolve earlier ACC and TRS reversal.
- [ ] Verify HC-05 RXD voltage tolerance and level-shifter output. Resolve 5 V HV issue before powering RXD.
- [ ] Verify transistor pinout and power-on/reset/power-loss PTT behaviour with collector disconnected from amplifier.
- [ ] Measure D7 high in RX and low in TX; confirm SEND settings for HF/50 MHz.
- [ ] Measure every CSV band target unloaded and loaded at amplifier; record actual XPA displayed band.
- [ ] Establish settling time and required radio TX delay by measurement; firmware has no explicit band-settle interlock.
- [ ] Confirm correct Bluetooth reconnect and frequency tracking on every intended band.
- [ ] Check unsupported frequencies inhibit PTT.
- [ ] Check loss of frequency updates, complete Bluetooth loss and controller reset inhibit keying. Note mode replies can keep the current freshness timer alive (source-review.md).
- [ ] Verify PTT release delay (15 ms source setting) and amplifier switching before RF arrives.
- [ ] Run low-drive RF tests into a suitable load per actual radio/amplifier manuals, then check RF immunity, SWR and heating.

Record date, hardware/module revisions, firmware hash, instrument, frequency, target mV, measured loaded mV, XPA band, RX/TX result and tester. Do not tick items until actually tested.

The earlier user-supplied voltage sequence did not include an unambiguous band-labelled measured table and differed from source defaults. It is not substituted for calibration data. CSV lists source defaults only.
