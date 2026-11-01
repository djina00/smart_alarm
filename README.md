# Pametni kućni alarm

Projekat iz predmeta Internet stvari. Alarm otkriva neovlašćeni ulazak (PIR senzor), uključuje sirenu (zujalica) i šalje događaje na server preko MQTT-a.

## Hardver

ESP32-S3 (Waveshare ESP32-S3-DEV-KIT-N16R8), PIR senzor HC-SR501, modul piezo zujalice.

| Komponenta | Pin komponente | Pin ESP32-S3 |
|---|---|---|
| PIR | VCC | 5V |
| PIR | OUT | GPIO4 |
| PIR | GND | GND |
| Zujalica | VCC | 3V3 |
| Zujalica | I/O | GPIO5 (pali se na LOW) |
| Zujalica | GND | GND |

## Prevođenje i pokretanje

- ESP-IDF v5.5.6, ciljni čip `esp32s3`
- `idf.py build` pa `idf.py -p COMx flash monitor`
- Simulacija: Wokwi ekstenzija za VS Code, otvoriti `diagram.json` i pokrenuti „Wokwi: Start Simulator“
