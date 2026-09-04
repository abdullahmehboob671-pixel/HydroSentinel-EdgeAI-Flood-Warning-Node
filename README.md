# HydroSentinel: Off-Grid Edge-AI Early Warning System for Flash Floods

Low-cost, off-grid Edge-AI flood early warning node powered by ESP32-S3, Edge Impulse TinyML, and sub-GHz LoRa radio.

**Project ID:** P00516  
**Institution:** Ghulam Ishaq Khan Institute of Engineering Sciences and Technology  
**Focus Area:** Open Innovation  

---

## 📌 Executive Summary
Monsoon flash floods frequently destroy power grids and cellular infrastructure, causing standard warning systems to fail when needed most. **HydroSentinel** is a low-cost, completely off-grid, autonomous early warning node designed to operate independently of centralized infrastructure. By executing TinyML model inference directly at the sensor head, the system detects rapid water-level anomalies and broadcasts real-time alerts via sub-GHz LoRa radio during total blackouts.

---

## 🛠️ Key Technical Features
* **Zero-Cloud Edge Intelligence:** Runs quantized TinyML / Edge Impulse models directly on an **ESP32-S3** microcontroller for instant local hazard classification.
* **Sensor Fusion & Compensation:** Integrates a waterproof ultrasonic sensor (**JSN-SR04T**) paired with ambient temperature/humidity compensation (**DHT22**) to compute speed-of-sound adjustments and real-time rate-of-rise ($\text{cm/s}$) metrics.
* **Long-Range Sub-GHz Broadcast:** Utilizes a **433 MHz LoRa transceiver (SX1278)** to broadcast telemetry and risk alerts over long distances through heavy rain, foliage, and obstacles without cellular network dependencies.
* **Off-Grid Power Resilience:** Designed for low-power consumption paired with solar battery harvesting (TP4056 + Lithium-ion) for continuous 24/7 deployment in remote river basins.

---

## 🏗️ System Architecture & Workflow

1. **Data Acquisition:** The ESP32-S3 polls environmental and acoustic/distance telemetry at high frequencies.
2. **On-Device Feature Extraction:** Computes temperature-corrected distance and rate-of-rise vectors.
3. **Edge ML Inference:** Local TinyML model classifies flood state (`Normal`, `Warning`, `Hazard`).
4. **LoRa Telemetry Packet:** Constructs an encrypted, compact payload string containing state, confidence score, rate-of-rise, and RSSI metrics.
5. **Field Broadcast:** Transmits packets over 433 MHz LoRa to local siren units or field receiver nodes.

---

## 💻 Hardware Bill of Materials (BOM)
* **MCU:** ESP32-S3 (Dual-Core 32-bit LX7, Vector Instructions for Machine Learning)
* **LoRa Radio:** SX1278 / Ra-02 (433 MHz, SPI Interface, 20 dBm Output)
* **Water Level Sensor:** JSN-SR04T (Waterproof Ultrasonic Sensor)
* **Environmental Sensor:** DHT22 (Temperature & Humidity)
* **Power:** TP4056 Charging Controller + 3.7V Li-ion / Solar Panel

---

## 📂 Repository Structure
```text
├── Firmware/
│   ├── Transmitter_EdgeAI_Node/   # Paste your ESP32-S3 main sketch (.ino) here
│   └── Receiver_Gateway_Node/     # Paste your LoRa receiver sketch (.ino) here
├── Model/                         # Upload exported C++ model zip / Edge Impulse headers
├── Hardware/                      # Upload circuit schematics, pinout diagrams, or photos
├── .gitignore
├── LICENSE
└── README.md
