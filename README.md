# Optical Data Transmission System (Arduino)

## Overview
This project implements an optical communication system using an Arduino. It transmits and receives control and sensor data over a light link. An LED acts as the transmitter, converting digital frames into light pulses, and a Photoresistor (LDR) acts as the receiver, sampling those pulses back into digital data. 

The software includes a custom UART-like protocol, incorporating data encryption (XOR cipher) and error-checking (Fletcher-16 checksum) to ensure integrity and security over the optical channel.

## Features
- **Optical Communication**: Data transmission over visible light using an LED and an LDR.
- **Custom Protocol**: 9-byte frame structure framing payload between defined Start and Stop bytes.
- **Error Checking**: Fletcher-16 checksum validation for received frames to prevent corrupted data processing.
- **Encryption**: XOR position-indexed cipher over the data payload and checksum.
- **Non-blocking Execution**: State-machine-based execution loop allowing simultaneous transmitting, receiving, logic evaluation, and display updating.
- **I/O Control Mapping**:
  - **Push Button** $\rightarrow$ Buzzer
  - **Tilt Switch** $\rightarrow$ Indicator LED
  - **Potentiometer** (with weighted average noise reduction) $\rightarrow$ 7-Segment Display
  - **Joystick & Pushbutton** $\rightarrow$ Fan / Motor Speed Control (PWM with smooth acceleration and deadzone)

## Hardware Requirements
- Arduino Uno (or compatible board)
- Transmitting LED (Pin 2)
- Photoresistor / LDR module (Pin A0)
- 7-Segment Display (Segments `A-G` on Pins 7-13, Common Cathode on A2)
- DC Motor / Fan Module (PWM Pin 5)
- Analogue Joystick (X-axis on Pin A5, Button on Pin 6)
- Potentiometer (Pin A1)
- Push Button (Pin 3)
- Tilt Switch (Pin A3)
- Buzzer (Pin 4)
- Indicator LED (Pin A4)
- Current limiting resistors for LEDs + 7-segment display and pull-down/pull-up resistors as required.

## Frame Structure
Each frame consists of 9 bytes:

| Byte Index | Description | Value Range |
| --- | --- | --- |
| `0` | Start Byte | `0x70` ('p') |
| `1` | Push Button State | `0` or `1` |
| `2` | Tilt Switch State | `0` or `1` |
| `3` | Potentiometer Level | `0` to `9` |
| `4` | Fan ON/OFF Toggle | `0` or `1` |
| `5` | Fan Speed | `0` to `99` |
| `6` | Fletcher-16 Checksum (Sum 1) | `0x00` - `0xFF` |
| `7` | Fletcher-16 Checksum (Sum 2) | `0x00` - `0xFF` |
| `8` | Stop Byte | `0x71` ('q') |

*Note: Bytes `1` through `7` are encrypted using an XOR coordinate-based cipher mask during transmission.*

## Dependencies
- **[SevSeg Library](https://github.com/DeanIsMe/SevSeg)**: Required for multiplexing the 7-segment display without delay. Install via the Arduino IDE Library Manager.

## How to Run
1. Install the `SevSeg` library in your Arduino IDE.
2. Wire the hardware components according to the defined pins in `CSA_main.ino`.
3. Open `CSA_main.ino` in the Arduino IDE and upload it to your board.
4. Open the Serial Monitor (9600 baud) to view the receiver's decoded byte stream and validation errors.