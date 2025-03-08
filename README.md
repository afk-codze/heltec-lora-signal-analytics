# IoT Tiny Weather Station

A compact IoT weather station using **Heltec WiFi LoRa 32 V3**, **BME280**, **BH1750**, and **HALJIA Rain Sensor** for real-time environmental monitoring. The project is implemented using **Arduino IDE** and **LoRa** for wireless data transmission.

## Features
- **ESP32-based IoT Weather Station**
- **Real-time environmental data collection** (temperature, humidity, pressure, light intensity, and rainfall)
- **LoRa communication for long-range data transmission**
- **Compact and low-power design**

## Setup
### 1. Install Dependencies
Ensure you have **Arduino IDE** installed and follow these steps:
- Install **ESP32 board support** by Espressif (version **3.1.1** or later).
  
### 2. Select the Correct Board & Port
- In **Arduino IDE**, navigate to `Tools → Board → ESP32` and select **Heltec WiFi LoRa 32 (V3)**.
- Go to `Tools → Port` and choose the correct COM port:
  - **Windows:** `COMx` (e.g., `COM3`)
  - **Linux:** `/dev/ttyUSB0`
  
### 3. Fix Serial Port Permission Issues (Linux)
If you encounter permission issues while accessing the serial port, run the following command:
```bash
sudo chmod 666 /dev/ttyUSB0
```

## Finding the Maximum Sampling Frequency
Before analyzing the signal, determine how fast the **Heltec LoRa 32 (V3)** can sample the **Rain Sensor**.

### Code (`/sampling/MaximumSamplingFrequency.ino`)

### Expected Output
```plaintext
20:50:13.756 -> Maximum Sampling Frequency: 33719.70 Hz
```

## Contributing
Feel free to contribute by submitting pull requests or reporting issues.

## License
This project is licensed under the **MIT License**.
