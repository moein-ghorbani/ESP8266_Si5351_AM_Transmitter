# Si5351 AM Transmitter

ESP8266 + Si5351 based AM transmitter with real audio playback via PWM

## Hardware
- ESP8266 (NodeMCU)
- Si5351 Clock Generator
- ADE-1 Mixer (or any diode ring mixer)

## Wiring
| ESP8266 | Si5351 | ADE-1 |
|---------|--------|-------|
| GPIO1 (TX) | CLK0 | RF In |
| GPIO14 (D5) | - | LO/IF In |
| GND | GND | GND |

## Features
- Web UI for frequency control (10kHz - 160MHz)
- Real melody playback via PWM on GPIO14
- Volume control
- EEPROM frequency storage

## Usage
1. Upload code to ESP8266
2. Connect to WiFi: `ESP_Si5351_AP` (password: `12345678`)
3. Open browser: `http://192.168.4.1`
4. Set carrier frequency and start melody
5. Connect outputs to ADE-1 mixer for AM modulation

## Notes
- Si5351 output: 2mA drive strength
- PWM frequency: 40kHz (above audible range)
- Audio frequency range: 262Hz - 523Hz
