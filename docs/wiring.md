# Wiring and pinout

This documents the latest saved v1.1 manual and recovered source. Earlier notes used different GPIOs and reversed XPA pins 2/3: do not combine revisions. Connector viewing orientation and wire colours must be established by continuity on the actual cable; no geometric pin position is inferred here.

| Wemos label | ESP8266 GPIO | Function | Connection |
|---|---:|---|---|
| D1 | 5 | Amplifier PTT | 2 kΩ to NPN base; collector to XPA ACC 2 |
| D2 | 4 | Band PWM | 1 kΩ to band node; node to XPA ACC 3 |
| D5 | 14 | Bluetooth RX | HC-05 TXD to D5 |
| D6 | 12 | Bluetooth TX | Through appropriately rated interface to HC-05 RXD |
| D7 | 13 | SEND input | IC-705 SEND; external 10 kΩ pull-up to 3.3 V |

## PTT transistor

D1 → 2 kΩ → base. Base → 10 kΩ → common GND. Emitter → GND. Collector → XPA ACC pin 2. Use 2N2222A/PN2222A/BC337 only after checking the exact device E/B/C pinout. D1 high turns the transistor on and pulls amplifier PTT low. Do not connect an ESP GPIO directly to XPA PTT or actively drive it high.

## Band filter

D2 → 1 kΩ → band node → XPA ACC pin 3. 10 µF capacitor positive → band node; negative → GND. Previous 10 kΩ value was replaced in the latest manual. PWM is 5 kHz, range 1023, assumed full scale 3300 mV. Measure the loaded voltage; the software is open-loop.

## IC-705 SEND/ALC TRS

The latest saved manual records TIP=SEND, RING=ALC unused, SLEEVE=GND. SEND connects to D7, sleeve to common ground. It records approximately 3.0–3.3 V in RX and near 0 V in TX. Earlier project notes said ring=PTT; this conflict needs continuity confirmation on the installed cable. Do not use the tuner socket for this connection.

## XPA125B ACC

| Number | Latest v1.1 project documentation |
|---:|---|
| 1 | Not connected |
| 2 | Active-low PTT input; NPN collector |
| 3 | Band-voltage input; filtered D2 |
| 4 | ALC unused |
| 5 | Not connected |
| 6 | Common GND |

Verify numbered contacts from the actual connector documentation and continuity, including mating-face versus solder-side orientation. Do not rely on purple/red wires or “top left”.

## Bluetooth and supplies

HC-05 breakout VCC → suitable supply (the saved build used 5 V); GND → common GND; TXD → D5. STATE and EN are unused in normal operation. Wemos power is 5 V USB; ESP GPIOs are 3.3 V.

The saved build used a BSS138 board: LV=3.3 V, HV=5 V, D6→LV1, HV1→HC-05 RXD, both grounds common. This is an unresolved electrical item: a 5 V HV pull-up can expose RXD to 5 V. Confirm the exact HC-05 breakout RXD tolerance/interface before powering that connection. A breakout VCC regulator alone does not establish RXD tolerance. This file does not certify that arrangement.

Common ground joins Wemos, HC-05, both interface ground terminals, transistor emitter, capacitor negative, IC-705 sleeve, XPA ACC 6 and supply ground.
