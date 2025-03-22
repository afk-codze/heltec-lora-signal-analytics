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

3. **Phase 3 – Compute Aggregate Over a Window**  
We compute a **rolling average** of the sampled data over a short time window (e.g., 0.1 s).

4. **Phase 4 – MQTT Transmission to an Edge Server**  
We **publish** the aggregated (rolling average) data to a **nearby edge server** via **MQTT** over Wi-Fi, enabling **real-time monitoring** and **seamless integration** with other analytics or dashboards.

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
![image](https://github.com/user-attachments/assets/b7019505-eb7d-4512-aff0-eea462d6f666)


### Phase 4: MQTT Transmission to an Edge Server

In this phase, we **transmit the rolling average** value computed in Phase 3 to an **edge server** using **MQTT** over Wi-Fi. We introduce **WiFi connectivity** and **MQTT publishing** into our existing FreeRTOS tasks, ensuring that the **aggregated data** is sent in real time as it’s computed. For demonstration, we use **Adafruit IO** as a convenient MQTT platform, but this approach works similarly with other MQTT brokers.

**Key Steps**:
1. **Connect to WiFi** – Provide your SSID and password so the ESP32 can join your local network.  
2. **Initialize MQTT** – Configure the MQTT client (e.g., AdafruitIO_WiFi or PubSubClient) with broker credentials (username, key, or password).  
3. **Publish the Aggregate** – After computing the rolling average, publish it to an MQTT topic (e.g., `sensor-rolling-average`).  
4. **Monitor** – Subscribe to the same MQTT topic from a dashboard or command-line client to verify data is arriving.

**Code Reference**: [rolling-average-MQTT.ino](/aggregate-function-and-transmission/rolling-average-MQTT.ino)

**Outcome**:  
By integrating **MQTT** into the sampling/aggregation task, the **rolling average** value is now **transmitted in real time** to a nearby (or cloud-based) edge server. This provides a **low-overhead** way to feed the processed data into dashboards, analytics engines, or any system that can subscribe to MQTT topics.
![image](https://github.com/user-attachments/assets/31257d0b-24b4-4665-8465-88f9152a1fdb)


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
- install **Adafruit IO Arduino** by Adafruit (version **4.3.0** or later)

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
