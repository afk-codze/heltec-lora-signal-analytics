# Adaptive Heltec WiFi LoRa V3 Signal analytics

This repository demonstrates how to:

- **Capture** a sensor signal at high speed on a Heltec WiFi LoRa V3 board (ESP32-based).
- **Analyze** the signal with an FFT to determine its maximum frequency and compute the best frequency to reproduce the signal with the nyquist bla bla.
- **Adapt** the sampling frequency dynamically based on the real-time signal characteristics to save power and reduce overhead.
- **Aggregate** Compute the average of the sampled signal over a window ...

> “Why sample at super-high frequency if your signal doesn’t need it?  
> Let’s adapt and save energy!”

---

## Overview

1. **Phase 1** – **Determine Maximum Sampling Frequency**  
   We start by finding out how many samples per second (Hz) our board can realistically capture. This provides the upper limit for all subsequent steps.

2. **Phase 2** – **FFT and Adaptive Sampling**  
   After we know our maximum sampling frequency, we sample the signal at that rate (initially), perform an FFT, detect the highest frequency in the signal, and adjust the sampling frequency accordingly (following Nyquist’s theorem).

3. **Phase 3** – **Compute Aggregate Over a Window**  
   TBD

4. **Phase 4** – **MQTT Transmission to an Edge Server**  
   TBD

5. **Phase 5** – **LoRaWAN Transmission to the Cloud**  
   TBD

---

## Project Phases in Detail

### Phase 1: Determine Maximum Sampling Frequency

In this phase, we measure how fast the Heltec WiFi LoRa V3 board can acquire samples. We continuously read from the ADC in a tight loop for 1 second, then count the total number of samples collected. Converting that count to “samples per second” (Hz) gives us the board’s maximum practical sampling rate.

**Code Reference**: [maximum-frequency.ino](/sampling/maximum-frequency.ino)

**Outcome**:  
By sampling on ADC pin **7**, we observed an approximate **32.200 Hz** sampling rate. This value is our baseline “maximum sampling frequency,” guiding how we set the upper bound for all subsequent phases in this project.


---

### Hardware & Software Requirements

- **Heltec WiFi LoRa V3** (ESP32-based)
- **Arduino IDE** (with Heltec board packages installed)
- **FreeRTOS** (already included in the ESP32 Arduino core)
- **MQTT Broker** (any local broker, e.g., Mosquitto, or external service)
- **LoRaWAN** TBD
- **Cloud infrastructure** TBD

---

## Setup

### 1. Install Dependencies

Ensure you have **Arduino IDE** installed and follow these steps:

- Install **ESP32 board support** by Espressif (version **3.1.1** or later).

### 2. Select the Correct Board & Port

- In **Arduino IDE**, navigate to `Tools → Board → ESP32` and select **Heltec WiFi LoRa 32 (V3)**.
- Go to `Tools → Port` and choose the correct COM port:
  - **Linux:** `/dev/ttyUSB0`

### 3. Fix Serial Port Permission Issues (Linux)

If you encounter permission issues while accessing the serial port, run the following command:

```bash
sudo chmod 666 /dev/ttyUSB0

---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---

## License

This project is open-source. You can use it in your own IoT adventures—just be sure to pay it forward and give credit where it’s due. 

---
