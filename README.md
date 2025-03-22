# Adaptive Heltec WiFi LoRa V3 Signal analytics

This repository demonstrates how to build an **IoT system** on a **Heltec WiFi LoRa V3 (ESP32)** board with **FreeRTOS**, capable of:

- **Capturing** a sensor signal (e.g., sums of sine waves like `2*sin(2π*3t) + 4*sin(2π*5t)`) at a high sampling rate.
- **Analyzing** the signal locally using an **FFT** to determine the highest frequency component and **adapt** the sampling frequency (per **Nyquist’s theorem**) to save energy and reduce overhead.
- **Aggregating** the signal data by computing an average (or other metrics) over a specified time window (e.g., 5 seconds).
- **Transmitting** the aggregated value to a nearby edge server via **MQTT over Wi-Fi**.
- **Relaying** that same aggregated value to the cloud using **LoRaWAN** and **TTN** for long-range, low-power connectivity.

> **Goal**: Demonstrate an IoT workflow that collects, processes, and transmits sensor data efficiently, adjusting the sampling rate in real time to balance **fidelity** and **power consumption**.

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

**Code Reference**: [maximum-theoretical-frequency.ino](/sampling/maximum-theoretical-frequency.ino)

**Outcome**:
By sampling on ADC pin **7**, we observed an approximate **32.200 Hz** sampling rate. This value is our baseline “maximum sampling frequency,” guiding how we set the upper bound for all subsequent phases in this project.

### Phase 2: FFT and Adaptive Sampling

In this phase, we initially tried using our **theoretical maximum** (around 32,200 Hz) from Phase 1. Once we added the **real workloads**—including FFT computations and other tasks—our system became **unreliable** at that rate. Through experimentation, we found that **5 kHz** was both **stable** and **sufficient** for our signal needs (up to ~2.5 kHz, following Nyquist’s rule).

**Code Reference**: [fft-and-adaptive-sampling.ino](/sampling/fft-and-adaptive-sampling.ino)

**Outcome**:  
We set a **5 kHz** sampling frequency as our practical upper bound for capturing and analyzing signals with the ESP32. This rate balances **signal fidelity** with the **processing overhead** needed.  

When testing our **simulated signal**, we identified a **maximum frequency component** of ~205.19 Hz in the FFT output. By **Nyquist’s theorem**, the **minimum** sampling frequency to reconstruct this signal without aliasing is **2 × 205.19 Hz ≈ 410.38 Hz**.  

> **TODO**: Return to this phase for a **more systematic approach** in selecting the optimal high-end sampling frequency, rather than relying solely on manual trial-and-error.

### Phase 3: Compute Aggregate Over a Window

In this phase, we aggregate our sensor data by computing a **rolling average** over a **0.1‑second window**. Our implementation uses two dedicated **FreeRTOS tasks**: one task generates a 200 Hz sine wave at an internal simulation rate of 5 kHz, and another task samples that generated signal at approximately 410 Hz. The sampling task uses a ring buffer (storing roughly 41 samples) along with a running sum to efficiently compute the rolling average as new samples arrive and the oldest ones are discarded.

**Code Reference**: [rolling-average.ino](/aggregate-function-and-transmission/rolling-average.ino)

**Outcome**:  
The outcome is a **continuous rolling average signal** computed over a 0.1‑second window.


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
- Install **arduinoFFT** by Enrique Condes (version **2.0.4** or later).

### 2. Select the Correct Board & Port

- In **Arduino IDE**, navigate to `Tools → Board → ESP32` and select **Heltec WiFi LoRa 32 (V3)**.
- Go to `Tools → Port` and choose the correct COM port:
  - **Linux:** `/dev/ttyUSB0`

### 3. Fix Serial Port Permission Issues (Linux)

If you encounter permission issues while accessing the serial port, run the following command:

```bash
sudo chmod 666 /dev/ttyUSB0
```
---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---

## License

This project is open-source. You can use it in your own IoT adventures—just be sure to pay it forward and give credit where it’s due.

---
