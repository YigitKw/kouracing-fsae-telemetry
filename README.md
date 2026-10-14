# KOU Racing - FSAE Telemetry System

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

Real-time telemetry and data acquisition system developed for a Formula Student Electric racing vehicle. The architecture comprises three principal subsystems: an on-vehicle digital instrument cluster (STM32), an Ethernet telemetry gateway (Raspberry Pi), and a trackside monitoring application (Android).

---

## Architecture Overview

The end-to-end data pipeline operates across three interconnected layers:

```mermaid
flowchart LR
    subgraph Vehicle["Vehicle Cluster (STM32F746G-DISCO)"]
        Sensors["Sensor Feed\n(RPM, Temp, Volt, Power)"] --> LVGL["Cockpit Display\n(480x272 LTDC / LVGL 9)"]
        Sensors --> UDP["LwIP UDP Client\n(10 Hz JSON Stream)"]
    end

    subgraph Gateway["On-Board Gateway (Raspberry Pi 5)"]
        UDPRecv["UDP Receiver\n(Port 5000)"] --> WS["WebSocket Hub\n(Port 3005)"]
        WS --> Tunnel["Cloudflare Tunnel\n(Remote LTE Link)"]
    end

    subgraph Trackside["Trackside Monitor (Android)"]
        Tunnel --> Client["WebSocket Client\n(OkHttp Stream)"]
        Client --> UI["Telemetry Dashboard\n(Real-time Cards & Diagnostics)"]
    end

    UDP -->|"Ethernet 100Mbps\n192.168.1.10 -> 192.168.1.2"| UDPRecv
    WS -->|"Local Wi-Fi\nws://192.168.1.2:3005"| Client
```

---

## Subsystems

### 1. Vehicle Cluster Firmware (`Telemetry/`)
* **Target Hardware:** STM32F746G-DISCO (ARM Cortex-M7 @ 216 MHz, 1 MB Flash, 320 KB SRAM, 64 Mbit external SDRAM).
* **Display Pipeline:**
  * 4.3" 480x272 LCD driven via LTDC (Layer 0, 16-bit RGB565 format at `0xC0000000` framebuffer base).
  * LVGL 9 graphics library configured for direct framebuffer flushing.
  * FMC SDRAM interface clocked at 100 MHz with CAS latency 3.
* **Operating System and Networking:**
  * FreeRTOS kernel manages UI rendering and telemetry dispatch tasks.
  * LwIP TCP/IP stack operates over an RMII Ethernet PHY with static IP `192.168.1.10`.
  * Telemetry frames dispatched at 10 Hz via UDP port 5000.
* **Driver Interface:**
  * Dual-screen support: Primary cockpit dashboard (RPM LED bar, traction control indicators, speed, temperatures, battery status) and secondary detailed diagnostics screen toggled via onboard user button (PI11).

### 2. Trackside Android Application (`TelemetryAndroid/`)
* **Platform:** Native Android (Kotlin, Min SDK 24, Target SDK 34).
* **Networking:** OkHttp WebSocket client supporting two operational modes:
  * **Local Network Mode:** Direct connection (`ws://<GATEWAY_IP>:3005`) for pit-lane Wi-Fi monitoring.
  * **Tunnel Mode:** Remote connection over cellular LTE via Cloudflare Zero Trust Tunnel endpoint (`wss://...`).
* **Authentication:** Team token challenge handshake on connection establishment prior to streaming packet reception.
* **Telemetry Decoder:** Parses 28-byte little-endian IEEE-754 payload buffers directly into structured telemetry state models.

### 3. Gateway and Utilities
* **`rpi_gateway_auth.js`:** Node.js WebSocket authentication middleware that verifies team credentials and terminates unauthenticated client sessions.
* **`udp_test_sender.py`:** Python utility that generates synthetic Formula Student telemetry streams over UDP to verify firmware and mobile software independently of vehicle hardware.

---

## Telemetry Packet Specification

### UDP Datagram (STM32 -> Gateway)
The STM32 firmware broadcasts JSON datagrams to the gateway over UDP port 5000 at 10 Hz:

```json
{
  "rpm": 5420,
  "speed": 82,
  "motor_temp": 68,
  "battery_voltage": 358.4,
  "throttle": 0.72,
  "brake": 0.00,
  "bat_temp": 42,
  "inv_temp": 48,
  "oil_temp": 78,
  "wtr_temp": 62,
  "tire_left": 58,
  "tire_right": 61,
  "power_watts": 185,
  "bat_percent": 74,
  "regen_percent": 12,
  "tcs_percent": 5,
  "mode": "ENDR",
  "ready": "true",
  "fault": "false",
  "traction_control": "false"
}
```

### Binary WebSocket Frame (Gateway -> Android)
Binary payloads streamed to the Android client are serialized into 28-byte little-endian buffers:

| Offset (Bytes) | Field | Type | Scale / Range |
|---|---|---|---|
| `0 - 3` | Sequence ID | uint32_t | Monotonic packet counter |
| `4 - 7` | Motor RPM | float (IEEE-754) | 0 - 10000 RPM |
| `8 - 11` | Vehicle Speed | float (IEEE-754) | 0 - 150 km/h |
| `12 - 15` | Motor Temperature | float (IEEE-754) | °C |
| `16 - 19` | Battery Voltage | float (IEEE-754) | 300.0 - 400.0 V |
| `20 - 23` | Throttle Input | float (IEEE-754) | 0.0 - 1.0 (or 0 - 100%) |
| `24 - 27` | Fault Status | uint32_t | 0 = OK, 1 = Fault |

---

## Repository Structure

```text
├── README.md                      # System documentation
├── LICENSE                        # MIT License
├── .gitignore                     # Build and environment exclusions
├── rpi_gateway_auth.js            # Gateway authentication middleware
├── udp_test_sender.py             # Telemetry simulation script
├── Telemetry_Final_Project.pdf    # Project documentation and specifications
├── Telemetry/                     # STM32CubeIDE project (C / FreeRTOS / LVGL)
│   ├── Core/                      # Application sources and HAL initialization
│   ├── Drivers/                   # CMSIS and STM32F7xx HAL drivers
│   ├── LWIP/                      # LwIP network configuration and target drivers
│   ├── Middlewares/               # FreeRTOS, LwIP, and LVGL library
│   └── Telemetry.ioc              # STM32CubeMX device configuration
└── TelemetryAndroid/              # Android Studio project (Kotlin)
    └── app/src/main/
        ├── java/com/kouracing/    # Socket manager, telemetry models, activity
        └── res/                   # Layouts, colors, themes, resources
```

---

## Build and Setup Guide

### Firmware (STM32)
1. Open **STM32CubeIDE** and import `Telemetry/` as an Existing Project into Workspace.
2. Verify GCC compiler settings for ARM Cortex-M7 with hardware floating-point support (`-mfpu=fpv5-sp-d16 -mfloat-abi=hard`).
3. Connect the STM32F746G-DISCO board via the ST-LINK USB interface (CN14).
4. Build the target (`Project -> Build Project`) and flash via standard ST-LINK debugger configuration.

### Android Application
1. Open **Android Studio** and import the `TelemetryAndroid/` directory.
2. Allow Gradle to synchronize project dependencies (OkHttp, AndroidX, Material3).
3. Connect an Android target device or launch an emulator with network access enabled.
4. Run the `:app` configuration.

### Telemetry Simulator
To test the instrument cluster and mobile application without physical vehicle sensors:
```bash
python udp_test_sender.py
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
