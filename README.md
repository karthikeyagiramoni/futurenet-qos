# FutureNet QoS — M05

**Deadline/Priority-Aware Queueing for ns-3.46.1**

## Overview

**FutureNet QoS** is a custom ns-3 module that implements priority-aware packet scheduling and deadline-aware queue management. It provides configurable scheduling policies, packet metadata, deadline-miss handling, queue-limit enforcement, tracing, and a configurable example program for evaluating QoS behavior under different traffic conditions.

---

## Features

- Priority-aware scheduling (priorities **0–7**, where **0** is highest)
- Four scheduling modes:
  - **0** – Strict Priority
  - **1** – Earliest Deadline First (EDF)
  - **2** – Hybrid (Strict Priority between classes + EDF within each class)
  - **3** – Weighted Round Robin (WRR)
- Deadline-miss detection
- Configurable expired-packet policy (Drop or Transmit)
- Global queue-limit enforcement
- Packet metadata using custom tags
- Queue tracing for:
  - Deadline misses
  - Expired packet drops
  - Dequeue events with queue delay
- Comprehensive unit tests

---

## Requirements

- ns-3.46.1
- C++ compiler supported by ns-3
- CMake
- Standard ns-3 build tools

---

## Project Structure

```
contrib/futurenet-qos/
├── CMakeLists.txt
├── README.md
├── model/
├── helper/
├── examples/
├── test/
├── doc/
│   └── FutureNet-QoS-Project-Report.md
└── results/
    ├── *.csv
    └── graphs/
```

---

## Build

From the **ns-3.46.1** root directory:

```bash
./ns3 configure
./ns3 build
```

---

## Running the Example

### Strict Priority

```bash
./ns3 run "futurenet-qos-example --schedulingMode=0"
```

### EDF

```bash
./ns3 run "futurenet-qos-example --schedulingMode=1"
```

### Hybrid

```bash
./ns3 run "futurenet-qos-example --schedulingMode=2"
```

### WRR

```bash
./ns3 run "futurenet-qos-example --schedulingMode=3"
```

### Example Parameters

| Option | Description |
|---------|-------------|
| `--queueLimit` | Maximum queue capacity |
| `--arrivalIntervalMs` | Packet arrival interval |
| `--serviceIntervalMs` | Packet service interval |
| `--durationMs` | Experiment duration |
| `--packetCount` | Number of packets generated |
| `--expiredPolicy` | `0=Drop`, `1=Transmit` |
| `--csvFile` | Output CSV filename |
| `--fairnessTest` | Enable controlled WRR fairness experiment |

Display all available options:

```bash
./ns3 run "futurenet-qos-example --PrintHelp"
```

---

## Running Tests

Run the FutureNet QoS test suite:

```bash
./test.py -s futurenet-queue-disc
```

Individual tests:

```bash
./ns3 run "test-runner --suite=futurenet-traffic-tag --verbose"
./ns3 run "test-runner --suite=futurenet-deadline-tag --verbose"
./ns3 run "test-runner --suite=futurenet-queue-disc --verbose"
```

All FutureNet QoS tests pass successfully.

---

## Experimental Results

### Congested Workload (1 ms arrival / 2 ms service)

| Mode | Throughput (bps) | Deadline Misses |
|------|-----------------:|----------------:|
| Strict Priority | 179,200 | 23 |
| EDF | **214,400** | **10** |
| Hybrid | 185,600 | 21 |
| WRR | 188,800 | 25 |

**Observations**

- EDF achieved the highest throughput.
- EDF recorded the fewest deadline misses.
- Strict Priority provided the lowest queue delay.
- WRR demonstrated weighted service allocation.

### WRR Fairness Validation

Configured weights: **4 : 3 : 2 : 1**

Measured transmissions:

| Priority | Packets |
|---------:|--------:|
| 0 | 1999 |
| 1 | 1501 |
| 2 | 1000 |
| 3 | 500 |

The measured distribution closely matches the configured WRR weights.

---

## Performance Graphs

### Throughput Comparison

![Throughput](results/graphs/throughput-comparison.png)

### Deadline Misses

![Deadline Misses](results/graphs/deadline-misses.png)

### Average Queue Delay

![Queue Delay](results/graphs/average-queue-delay.png)

### WRR Fairness

![WRR Fairness](results/graphs/wrr-fairness.png)

---

## Documentation

The complete project report is available at:

- **Project Report:** `doc/FutureNet-QoS-Project-Report.md`

The repository also includes measured CSV files inside `results/`.

---

## Project Objective

FutureNet QoS (M05) investigates how packet priority and transmission deadlines influence queue scheduling, deadline handling, fairness, and quality of service within the ns-3.46.1 network simulator.

---

## Repository

GitHub:

`https://github.com/karthikeyagiramoni/futurenet-qos`