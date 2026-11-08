
# ESP32 Voice-Controlled LED Lights

A hands-free LED control system built using ESP32, ESP-IDF, Google Home, and MacroDroid.

## How It Works

Voice Command
    ↓
Google Gemini
    ↓
Google Home Automation
    ↓
Android Notification
    ↓
MacroDroid
    ↓
HTTP POST over Wi-Fi
    ↓
ESP32
    ↓
LED Control

## Hardware

- ESP32 development board
- Red, yellow, and green LEDs
- 220–330 ohm resistors
- Breadboard and jumper wires

## LED Connections

| LED | ESP32 GPIO |
|-----|------------|
| Red | GPIO18 |
| Yellow | GPIO19 |
| Green | GPIO21 |

Each LED is connected through a current-limiting resistor.

## Software

- ESP-IDF (C)
- ESP32 Wi-Fi station
- ESP-IDF HTTP server
- Google Home Automations
- MacroDroid

## HTTP API

Method: POST

Examples:

- `/api/led?color=red&state=on`
- `/api/led?color=red&state=off`
- `/api/led?color=green&state=on`
- `/api/led?color=yellow&state=on`
- `/api/led?color=all&state=off`

## Features

- Hands-free voice commands
- Wireless control over local Wi-Fi
- Independent LED control
- All LEDs OFF command
- No additional cloud service required on the ESP32

## Notes

Google Home and MacroDroid are configured separately on the Android phone.

The HTTP API is intended for use on a trusted local network.
