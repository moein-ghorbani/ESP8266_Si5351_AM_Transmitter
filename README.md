# ESP8266 Si5351 AM Transmitter

[![PlatformIO CI](https://github.com/yourusername/ESP8266_Si5351_AM_Transmitter/actions/workflows/platformio.yml/badge.svg)](https://github.com/yourusername/ESP8266_Si5351_AM_Transmitter/actions/workflows/platformio.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP8266](https://img.shields.io/badge/ESP8266-NodeMCU-blue)](https://www.espressif.com/en/products/socs/esp8266)

**Real AM Modulator using Si5351 + PWM Audio | Web-based Frequency Control**

---

## 🎯 Features

- ✅ **Si5351 Clock Generator** – RF carrier from 10kHz to 160MHz
- ✅ **Real Audio via PWM** – GPIO14 outputs actual musical notes (262Hz–523Hz)
- ✅ **Web Interface** – Real-time frequency control with Hz/kHz/MHz units
- ✅ **Preset Frequencies** – 10kHz, 100kHz, 1MHz, 10MHz, 50MHz, 100MHz
- ✅ **Volume Control** – Adjustable PWM amplitude (0–100%)
- ✅ **EEPROM Storage** – Last frequency saved after 5 seconds of inactivity
- ✅ **WiFi Auto-Reconnect** – Connects to `SSHP` network only (no AP fallback)
- ✅ **2mA Output Drive** – Cleaner signal, reduced harmonics

---

## 📡 How It Works
