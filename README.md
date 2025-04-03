# Adaptive Heltec WiFi LoRa V3 Signal analytics

This repository demonstrates how to build an **IoT system** on a **Heltec WiFi LoRa V3 (ESP32)** board with **FreeRTOS**, capable of:

- **Capturing** a sensor signal (e.g., sums of sine waves like `2*sin(2π*150t) + 4*sin(2π*200t)`) at a high sampling rate.
- **Analyzing** the signal locally using an **FFT** to determine the highest frequency component and **adapt** the sampling frequency (per **Nyquist’s theorem**) to save energy and reduce overhead.
- **Aggregating** the signal data by computing an average (or other metrics) over a specified time window (e.g., 0.1 seconds).
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

4. **Phase 4 – MQTT Transmission to an Edge Server over Wi-Fi**  
We **publish** the aggregated (rolling average) data to a **nearby edge server** via **MQTT** over Wi-Fi, enabling **real-time monitoring** and **seamless integration** with other services.

5. **Phase 5** – **LoRaWAN Transmission to the Cloud**  
   The aggregated data is transmitted to the cloud using LoRaWAN. This phase enables low-power, long-range communication suitable for remote or battery-powered deployments.

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

### Phase 3: Compute Aggregate Over a Window

In this phase, we aggregate our signal data by computing a **rolling average** over a **0.1‑second window**. Our implementation uses two dedicated **FreeRTOS tasks**: one task generates a composite signal made of 150 Hz and 200 Hz sine waves at an internal simulation rate of 5 kHz, and another task samples that generated signal at approximately 410 Hz. The sampling task uses a ring buffer (storing roughly 41 samples) along with a running sum to efficiently compute the rolling average as new samples arrive and the oldest ones are discarded.

**Code Reference**: [rolling-average.ino](/aggregate-function-and-transmission/rolling-average.ino)

**Outcome**:  
The outcome is a **continuous rolling average signal** computed over a 0.1‑second window.

![Screenshot From 2025-04-03 17-07-49](https://github.com/user-attachments/assets/872c60f0-ab9a-4d3d-a9ac-fcc47671cfa1)


### Phase 4: MQTT Transmission to an Edge Server over WiFi

In this phase, we **transmit the rolling average** value computed in Phase 3 to an **edge server** using **MQTT** over Wi-Fi. We introduce **WiFi connectivity** and **MQTT publishing** into our existing FreeRTOS tasks, ensuring that the **aggregated data** is sent in real time as it’s computed. For demonstration, we use **Adafruit IO** as a convenient MQTT platform, but this approach works similarly with other MQTT brokers.

**Key Steps**:
1. **Connect to WiFi** – Provide your SSID and password so the ESP32 can join your local network.  
2. **Initialize MQTT** – Configure the MQTT client (e.g., AdafruitIO_WiFi or PubSubClient) with broker credentials (username, key, or password).  
3. **Publish the Aggregate** – After computing the rolling average, publish it to an MQTT topic (e.g., `sensor-rolling-average`).  
4. **Monitor** – Subscribe to the same MQTT topic from a dashboard or command-line client to verify data is arriving.

**Code Reference**: [rolling-average-WiFi-MQTT.ino](/aggregate-function-and-transmission/rolling-average-WiFi-MQTT/rolling-average-WiFi-MQTT.ino)

**Outcome**:  
By integrating **MQTT** into the sampling/aggregation task, the **rolling average** value is now **transmitted** to a nearby (or cloud-based) edge server. This provides a **low-overhead** way to feed the processed data into dashboards, analytics engines, or any system that can subscribe to MQTT topics.

![image](https://github.com/user-attachments/assets/31257d0b-24b4-4665-8465-88f9152a1fdb)

---

### Phase 5: LoRaWAN Uplink to The Things Network (TTN) and MQTT Subscription

In this phase, we replace the MQTT-based Wi-Fi transmission with **LoRaWAN** + MQTT. The device now transmits the **rolling average**, computed in Phase 3, over **LoRaWAN** to **The Things Network (TTN)**—a public, decentralized LoRaWAN infrastructure.

The rolling average is sent as a **4-byte float uplink** using **OTAA (Over-The-Air Activation)**. The **Heltec LoRaWAN library** manages the join request, uplink scheduling, and network interaction.

**Code Reference**: [rolling-average-LoRa-MQTT.ino](aggregate-function-and-transmission/rolling-average-LoRa-MQTT/rolling-average-LoRa-MQTT.ino)

### Key Steps

1. **Register on TTN**  
   - Go to [TTN Console](https://console.thethingsnetwork.org/), select your region, and **create an account**.  
   - Create an **Application**, then **Register a Device** to obtain your LoRaWAN credentials:  
     - `DevEUI`  
     - `AppEUI`  
     - `AppKey`  

2. **Configure Arduino IDE for LoRaWAN**  
   - Install the **Heltec ESP32 Series Dev-Boards** and the **Heltec ESP32 Dev-Boards library**.  
   - Select the board:  
     `Tools → Board → Heltec ESP32 Series Dev-Boards → WiFi LoRa 32 (V3)`  
   - Set the LoRaWAN region:  
     `Tools → LoRaWAN Region → REGION_EU868` (or your specific region)

3. **Add LoRaWAN Credentials**  
   - Store `DevEUI`, `AppEUI`, and `AppKey` in a `secrets.h` like seen in: [secrets-example.h](aggregate-function-and-transmission/rolling-average-LoRa-MQTT/secrets-example.h)
   - Configure the sketch to use **OTAA** for joining the TTN network.

4. **Join the Network and Transmit**  
   - On boot, the device sends a join request to TTN.  
   - Once joined, it transmits the **rolling average** value periodically, encoded as a **4-byte float**.  
   - Transmission occurs within a FreeRTOS task to ensure proper timing and multitasking.

5. **Subscribe to Uplink Messages via MQTT**  
   - TTN provides MQTT access to device uplinks. To retrieve the necessary connection details (such as broker URL, username, topic, and API key), navigate to the Integrations → MQTT section of your TTN Console. There, you’ll find all the 
     credentials needed to subscribe to device messages using an external MQTT client.

     ![image](https://github.com/user-attachments/assets/5b03b54f-ad5f-48b9-9e23-6e82f6e49f7f)

   - We use a simple Bash script to subscribe to the uplink topic:  

     ```bash
     #!/bin/bash

     BROKER="eu1.cloud.thethings.network"
     PORT=1883
     USERNAME="heltec-lora-signal-analytics@ttn"
     PASSWORD="YOUR_API_KEY"  # Replace this with your actual API key
     TOPIC="v3/heltec-lora-signal-analytics@ttn/devices/+/up"

     mosquitto_sub -h "$BROKER" -p "$PORT" -u "$USERNAME" -P "$PASSWORD" -t "$TOPIC"
     ```

   - This script subscribes to all device uplinks within the application (/+/). The received payload includes metadata and the encoded float.

      ```
      {
        "end_device_ids": {
          "device_id": "heltech-new",
          "application_ids": {
            "application_id": "heltec-lora-signal-analytics"
          },
          "dev_eui": "***",
          "join_eui": "***",
          "dev_addr": "***"
        },
        "correlation_ids": [
          "gs:uplink:***"
        ],
        "received_at": "2025-04-02T18:06:01.475695399Z",
        "uplink_message": {
          "session_key_id": "***",
          "f_port": 2,
          "frm_payload": "***",
          "decoded_payload": {
            "rolling_avg": 6.378973960876465
          },
          "rx_metadata": [
            {
              "gateway_ids": {
                "gateway_id": "spv-rooftop-panorama",
                "eui": "***"
              },
              "timestamp": 3025567108,
              "rssi": -119,
              "channel_rssi": -119,
              "snr": -3.5,
              "location": {
                "latitude": 41.8937,
                "longitude": 12.49399,
                "altitude": 55,
                "source": "SOURCE_REGISTRY"
              },
              "uplink_token": "***",
              "channel_index": 6,
              "received_at": "2025-04-02T18:06:01.247738664Z"
            }
          ],
          "settings": {
            "data_rate": {
              "lora": {
                "bandwidth": 125000,
                "spreading_factor": 7,
                "coding_rate": "4/5"
              }
            },
            "frequency": "867700000",
            "timestamp": 3025567108
          },
          "received_at": "2025-04-02T18:06:01.270786763Z",
          "confirmed": true,
          "consumed_airtime": "0.051456s",
          "version_ids": {
            "brand_id": "heltec",
            "model_id": "wifi-lora-32-class-c-otaa",
            "hardware_version": "_unknown_hw_version_",
            "firmware_version": "1.0",
            "band_id": "EU_863_870"
          },
          "network_ids": {
            "net_id": "***",
            "ns_id": "***",
            "tenant_id": "ttn",
            "cluster_id": "eu1",
            "cluster_address": "eu1.cloud.thethings.network"
          }
        }
      }
      
      ```


6. **Decode the Payload in TTN Console**  
   - In the TTN Application, add a **Payload Formatter** (e.g., using JavaScript) to convert the 4-byte float into a human-readable number.
   **Code Reference**: [ttn-payload-formatter.js](aggregate-function-and-transmission/rolling-average-LoRa-MQTT/utils/ttn-payload-formatter.js)

### Outcome

With LoRaWAN uplink now integrated into the system, the rolling average is transmitted over a **long-range, low-power network**, enabling deployment in **remote, battery-powered, or off-grid environments**. At the same time, **MQTT access to TTN uplinks** allows you to monitor transmissions locally or integrate them with your existing data pipelines.

---

## Energy Consumption and Duty Cycle Analysis

This section analyzes the energy profile of the current implementations, where real-time tasks run continuously and periodic data transmissions are used to report sensor-like readings. The system is evaluated in two wireless communication configurations: **LoRaWAN** and **Wi-Fi**.

In both modes, the ESP32 executes:
- A **signal generation task**, generating a composite signal made of 150 Hz and 200 Hz sine waves at an internal simulation rate of 5 kHz.
- A **sampling task**, operating at ~410 Hz, that computes a 0.1-second rolling average

The way data is transmitted, and the impact on power consumption, varies greatly between the two communication modes.

### LoRa Mode

The FreeRTOS tasks are always active, resulting in a steady power draw from the CPU and memory. However, the wireless transceiver is only enabled during transmission. Between transmissions, the system enters **modem sleep**, a low-power state in which the CPU continues to operate while the radio remains off. In this state, the device draws approximately **20 mA** at most.

Every 15 seconds, the device transmits a small payload over LoRa containing the computed rolling average (a 4-byte float). This triggers a short spike in power usage, reaching at most **260 mA** during transmission. 

The duration of each LoRa transmission (time-on-air) was calculated based on the LoRaWAN physical layer settings used in this implementation. We set the default data rate (DR) to 3

```cpp
LoRaWAN.setDefaultDR(3);
```

According to the LoRaWAN Regional Parameters for the **EU868** band, **DR3** corresponds to:​

   - **Spreading Factor (SF)**: 9  
   - **Bandwidth (BW)**: 125 kHz

Given these parameters, the region that we are currently using and the size of the payload, we are able to estimate the time-on-air using [TTN LoRaWAN airtime calculator](https://www.thethingsnetwork.org/airtime-calculator/)

![image](https://github.com/user-attachments/assets/58021857-dac2-49cc-84bc-825b6a18ec83)

So our duty cycle looks like this:

![3f8a5ecf-4b5d-4c2b-8462-05d92fa7bb0d](https://github.com/user-attachments/assets/ec3c4b4e-dbf1-45b5-ab2c-53f0a6915e55)

To avoid synchronized transmissions with other devices, a small randomized offset is applied to each transmission interval:

```cpp
txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
```

This introduces jitter around the 15-second base interval, reducing the likelihood of network collisions in multi-device environments.

### Wi-Fi Mode

In the Wi-Fi configuration, the ESP32 again runs the same two tasks for signal generation and rolling average computation. However, in this mode, the average value is **published continuously** to **Adafruit IO** using the MQTT protocol, resulting in a very different power profile.

- The **sampling task** operates at ~410 Hz (every 2.44 ms) and calls `avgFeed->save(...)` at each iteration.
- While the `save()` call is lightweight in code, it triggers **frequent network activity**, especially if Adafruit IO accepts high-frequency publishing.

The device remains in **active Wi-Fi mode** continuously:
- **Wi-Fi Idle Current**: ~80 mA (ESP32 connected to Wi-Fi, not transmitting)
- **Transmission Spikes**: ~260 mA every ~0.5 seconds, each lasting ~150 ms (based on MQTT behavior and practical measurements)

These frequent spikes result in a high-duty-cycle transmission pattern, clearly visible in the energy profile:

![output](https://github.com/user-attachments/assets/f84bee48-c49b-4a90-992f-7905d506480d)

---

## Latency Analysis

### LoraWan:

In LoRaWAN, devices are categorized into different classes to accommodate various communication needs and power consumption requirements. **Class A** (our case) devices are the most basic and energy-efficient type. They operate on an **Aloha-like protocol**, where the device initiates communication by sending data to the network (uplink). Immediately following this uplink transmission, the device opens two short receive windows:

1. **First Receive Window (RX1):** Opens 5 second after the uplink ends.
2. **Second Receive Window (RX2):** Opens 1 seconds after the RX1 ends.

Source: https://www.thethingsnetwork.org/docs/lorawan/classes/#class-a

![685178bb-370a-4ebf-8ff7-89bad5cb631d](https://github.com/user-attachments/assets/5e774094-9b1b-4236-8aa9-e226aab4ee0d)

- Transmit Time on Air (ToA) = 164 ms
- The network sends the ACK in RX1 or RX2
- We are using confirmed uplinks with up to 4 retries

**Best Case (ACK in RX1 after first try):**
```
Latency = ToA + RX1 delay = 0.164 + 5 = 5.164 seconds
```

**Worst Case (ACK in RX1 after 4th try):**
```
Retry cycle is: ToA + RX1 delay + (ToA + RX1 delay + ...) repeated 4 times = 4 × (0.164 + 5) = 20.656 seconds
```

### Wi-Fi

To measure end-to-end latency over Wi-Fi, we timestamp each MQTT message on the ESP32 using `millis()` and publish it to the topic:

```
codzetest/feeds/send
```

A PC running `mosquitto_sub` listens to that topic and immediately echoes the message back to:

```
codzetest/feeds/echo
```

The ESP32, subscribed to the echo topic, receives its own message and computes the round-trip latency as:

```
latency = millis() - sent_timestamp
```

Example command to echo messages on the PC:

```bash
mosquitto_sub -h test.mosquitto.org -t "codzetest/feeds/send" | while read line; do
  mosquitto_pub -h test.mosquitto.org -t "codzetest/feeds/echo" -m "$line"
done
```

![Screenshot From 2025-04-03 11-31-41](https://github.com/user-attachments/assets/d664105d-eb9b-4b1c-bca3-692e015fae40)

Latency may vary based on network conditions.

---

### Hardware & Software Requirements

- **Heltec WiFi LoRa V3** (ESP32-based)
- **Arduino IDE** (with Heltec board packages installed)
- **FreeRTOS** (already included in the ESP32 Arduino core)
- **MQTT Broker** (any local broker, e.g., Mosquitto, or external service)
- **LoRaWAN** (Configured via The Things Network (TTN), using OTAA (Over-The-Air Activation))
- **Cloud infrastructure** TBD

---

## Setup

### 1. Install Dependencies

Ensure you have the **Arduino IDE** installed and follow these steps:

- Install **ESP32 board support** by Espressif (version **3.1.1** or later).
- Install **arduinoFFT** by Enrique Condes (version **2.0.4** or later).
- Install **Adafruit IO Arduino** by Adafruit (version **4.3.0** or later).
- Install **Adafruit GFX Library** by Adafruit.
- Add Heltec board support:
  - Go to **File → Preferences** → Add this URL to "Additional Board Manager URLs":
    ```
    https://resource.heltec.cn/download/package_heltec_esp32_index.json
    ```
  - Then go to **Tools → Board → Board Manager** and install **Heltec ESP32 Series Dev-Boards**.
  - Go to **Sketch → Include Library → Manage Libraries**, search for and install:
    - **Heltec ESP32 Dev-Boards** by Heltec.


### 2. Set Up The Things Network (TTN)

- Go to [https://console.thethingsnetwork.org/](https://console.thethingsnetwork.org/)  
- Choose your region and **create an account** (or log in).  
- Create an **Application** → inside the application, **Register a Device**.  
- You will receive your **DevEUI**, **AppEUI**, and **AppKey** (for OTAA) which go into your `secrets.h` file.


### 3. Select the Correct Board & LoRaWAN Region

- In the Arduino IDE:
  - Go to **Tools → Board → Heltec ESP32 Series Dev-Boards → WiFi LoRa 32 (V3)**.
  - Then go to **Tools → LoRaWAN Region** and select `REGION_EU868` (or your region).


### 4. Select the Correct COM Port

- Go to **Tools → Port** and select the port matching your device:
  - Linux: `/dev/ttyUSB0`

### 5. Fix Serial Port Permissions (Linux only)

If you encounter permission issues accessing the serial port, run:

```bash
sudo chmod 666 /dev/ttyUSB0
```

Or better (recommended):

```bash
sudo usermod -a -G dialout $USER
```
Then log out and back in.

---

## Contributing

Feel free to submit pull requests for improvements or open issues for any bugs or questions.

---

## License

This project is open-source. You can use it in your own IoT adventures—just be sure to pay it forward and give credit where it’s due.

---
