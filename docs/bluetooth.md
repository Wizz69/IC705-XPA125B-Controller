# HC-05 Bluetooth setup

Recovered setup refers to HC-05 Classic SPP firmware `3.0-20170601`, normal UART 9600,8,N,1. Another module reported `hc05V2.3_le`; it was not successful in the recorded setup. Do not infer compatibility from the HC-05 label alone.

## AT mode

Disconnect amplifier PTT. Use a separate USB/UART or pass-through test sketch, after checking UART voltage levels. The production IBAC sketch is not an AT-command bridge. Full AT mode normally uses 38400 baud; module button/KEY procedure and line endings depend on its firmware. Query:

```text
AT
AT+VERSION?
AT+ROLE?
AT+UART?
AT+NAME?
AT+ADDR?
```

Record responses first. The saved module used normal UART 9600; if needed and supported, set `AT+UART=9600,0,0`.

## Bind and pair

Replace `<radio-address>` with YOUR IC-705 address in HC-05 comma format. The previous station-specific address is deliberately not embedded in these instructions.

```text
AT+ROLE=1
AT+ROLE?
AT+BIND=<radio-address>
AT+PSWD="0000"
AT+CMODE=0
AT+PAIR=<radio-address>,20
```

On the IC-705 enable Bluetooth and Pairing Reception before PAIR; confirm matching passkey. The recorded setup used 0000. `AT+RMAAD` was used historically to clear stored pairings; it removes pairings, so only use it when intentionally resetting them. `AT+INQ` returned ERROR:(1F) on the saved module; pairing by known address avoided inquiry. Reboot in normal mode without holding the AT button. Use the radio's stored-device Connect command if needed.

Set IC-705 Bluetooth Data Device Set → Serialport Function → CI-V (Echo Back ON). Default CI-V address is A4, matching firmware. Confirm real frequency updates on the web page; LED patterns alone do not prove CI-V communication.
