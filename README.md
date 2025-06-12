# 💊 Pill Dispenser Project

This repository contains the implementation details for the Pill Dispenser, a second-year hardware project developed at Metropolia University of Applied Sciences by Xuan Dang, Mong Phan, and Gemma Qin, submitted on January 14, 2024. The project aims to create an automated pill dispenser to improve medication adherence by dispensing daily medications at preset times, with real-time monitoring and logging capabilities.

## 📖 Project Report

The full project report is available here [Pill_Dispenser_Report_G02.pdf](Pill_Dispenser_Report_G02.pdf).

## 📝 Project Overview

The Pill Dispenser addresses the challenge of medication non-adherence, particularly for elderly individuals or those with busy schedules. The device features an eight-compartment wheel (seven for daily medications, one for calibration) and operates autonomously to dispense pills at predefined intervals (30 seconds for testing, daily in practice). Key features include:

- **🔄 Automated Dispensing**: A stepper motor rotates the wheel to dispense pills, with calibration ensured by an optical sensor.
- **✅ Dispensing Validation**: A piezoelectric sensor confirms successful pill dispensing.
- **🌐 LoRaWAN Communication**: Transmits device status (e.g., pill dispensed, empty dispenser) to a server for real-time monitoring.
- **💾 Non-volatile Storage**: An I2C EEPROM stores device state and logs across power cycles.

The system uses a Raspberry Pi Pico WH as the core processing unit, paired with a Raspberry Pi Debug Probe for development. The project achieved its minimum requirements and partially met advanced goals, such as LoRaWAN integration and state persistence.

## 🛠 Prerequisites

### 🔧 Hardware
- Raspberry Pi Pico WH
- Raspberry Pi Debug Probe
- Elecrow Crowtail I2C EEPROM (256 kbits)
- Grove LoRa-E5 module
- 28BYJ-48 stepper motor with Elecrow ULN2003 driver
- Optical fork sensor
- Piezoelectric sensor
- 3D-printed dispenser base and wheel
- 6-pin JST and 4-pin Grove connectors
- Power supply (5V, suitable for Pico and motor)

## 🚀 Usage

1. **🤖 Run the Dispenser**:
   - Power on the Raspberry Pi Pico WH. The device waits for a button press, blinking an LED (GP20–GP22).
   - Press the button to calibrate the wheel, which rotates until the optical sensor detects the calibration compartment.
   - Press another button to start dispensing. The wheel rotates every 30 seconds (for testing), and the piezoelectric sensor validates pill dispensing.
   - If no pill is detected, the LED blinks five times. After seven dispenses, the system resets to calibration mode.

2. **📡 Monitor via LoRaWAN**:
   - Configure the LoRaWAN module using AT commands (see [LoRa-E5_AT_Command_Specification_V1.0.pdf](LoRa-E5_AT_Command_Specification_V1.0.pdf) for details):
     ```plaintext
     +MODE=LWOTAA
     +KEY=APPKEY,"<your-hex-key>"
     +CLASS=A
     +PORT=8
     +JOIN
     +MSG="status-message"
     ```
   - Run `lorareceive.py` on a computer connected to the LoRaWAN server:
     ```bash
     lorareceive.py
     ```
   - View device status updates (e.g., boot, pill dispensed, empty).

3. **📊 Check Logs**:
   - Device state and logs are stored in the I2C EEPROM and can be accessed via the firmware.

## 🧪 Testing and Validation

The system was tested for:
- **🔄 Calibration**: The optical sensor accurately aligned the wheel.
- **✅ Dispensing**: The piezoelectric sensor confirmed pill dispensing with high reliability.
- **🌐 Communication**: LoRaWAN successfully transmitted status updates, though with a 10-second delay per message.
- **💾 Persistence**: EEPROM stored device state across power cycles.

## ⚠️ Limitations and Future Work

- **🚫 Limitations**:
  - LoRaWAN’s low bandwidth limits real-time updates.
  - Mechanical issues caused occasional wheel misalignment.
  - No mobile app integration for user notifications.
- **🔮 Future Improvements**:
  - Integrate with a mobile app for reminders and monitoring.
  - Upgrade to a more robust stepper motor for reliability.
  - Optimize LoRaWAN messaging for faster updates.

---

## 📌 Contributors

- **Mong Phan**
- **Gemma Qin**
- **Xuan Dang**

---

## ⚖️ License

This project was developed for academic purposes at **Metropolia University of Applied Sciences**. Users can freely use this project for educational purpose ONLY.
