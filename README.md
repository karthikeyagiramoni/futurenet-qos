FutureNet QoS — M05

Deadline/Priority-Aware Queueing for ns-3.46.1

Overview

FutureNet QoS is a custom ns-3 module for priority-aware packet scheduling and deadline-aware queue management. It provides configurable scheduling policies, packet metadata, deadline-miss handling, queue-limit enforcement, tracing, and an example program for evaluating QoS behavior under different traffic conditions.

Features

- Priority-aware scheduling: Supports priority levels 0–7, where 0 is the highest priority.
- Scheduling modes:
  - 0 — Strict Priority
  - 1 — Earliest Deadline First (EDF)
  - 2 — Hybrid scheduling
  - 3 — Weighted Round Robin (WRR)
- Deadline handling: Detects missed deadlines and supports dropping or transmitting expired packets.
- Queue-limit enforcement: Configurable maximum number of queued packets.
- Packet metadata: Traffic tags carry priority, flow ID, and importance.
- Tracing: Supports dequeue, deadline-miss, and expired-packet-drop events.
- Testing: Includes queue-disc tests for supported scheduling and queue-management behavior.

Requirements

- ns-3.46.1
- A working C++ build environment supported by ns-3
- CMake and the build tools required by your ns-3 installation

Integration

Place the futurenet-qos module inside the ns-3 contrib/ directory:

ns-3.46.1/
└── contrib/
    └── futurenet-qos/
        ├── CMakeLists.txt
        ├── model/
        ├── test/
        ├── examples/
        ├── doc/
        ├── results/
        └── README.md

Build

From the root of the ns-3.46.1 source tree, run:

./ns3 configure
./ns3 build

To build the example:

./ns3 build futurenet-qos-example

Run the Example

Run Strict Priority scheduling:

./ns3 run "futurenet-qos-example --schedulingMode=0"

Run EDF scheduling:

./ns3 run "futurenet-qos-example --schedulingMode=1"

Run Hybrid scheduling:

./ns3 run "futurenet-qos-example --schedulingMode=2"

Run WRR scheduling:

./ns3 run "futurenet-qos-example --schedulingMode=3"

Configure the queue limit:

./ns3 run "futurenet-qos-example --schedulingMode=0 --queueLimit=3"

Other example parameters include arrivalIntervalMs, serviceIntervalMs, durationMs, packetCount, expiredPolicy, csvFile, and fairnessTest.

Run Tests

From the ns-3 source tree, run:

./test.py -s futurenet-queue-disc

Experimental Results

Congested workload with a 1 ms arrival interval, 2 ms service interval, 100 packets, and a queue limit of 20:

Strict Priority:
- Throughput: 179,200 bps
- Deadline misses: 23
- Average queue delay: 7.929 ms

Earliest Deadline First (EDF):
- Throughput: 214,400 bps
- Deadline misses: 10
- Average queue delay: 23.060 ms

Hybrid:
- Throughput: 185,600 bps
- Deadline misses: 21
- Average queue delay: 8.776 ms

Weighted Round Robin (WRR):
- Throughput: 188,800 bps
- Deadline misses: 25
- Average queue delay: 12.136 ms

In this congested experiment, EDF achieved the highest throughput and the fewest deadline misses. Strict Priority had the lowest average queue delay. These results depend on the tested workload and do not establish a universal ranking of scheduling policies.

WRR Fairness Experiment

The WRR experiment used weights of 4:3:2:1.

Measured transmitted packets:
- Priority 0: 1,999
- Priority 1: 1,501
- Priority 2: 1,000
- Priority 3: 500

The measured service shares were approximately 39.98%, 30.02%, 20%, and 10%, respectively, closely matching the expected weighted allocation.

Performance Graphs

The repository includes the following graphs in results/graphs/:

- throughput-comparison.png — throughput comparison across scheduling modes.
- deadline-misses.png — deadline misses across scheduling modes.
- average-queue-delay.png — average queue delay under congestion.
- wrr-fairness.png — expected and measured WRR service shares.

Project Report

The detailed project report is available at:

doc/FutureNet-QoS-Project-Report.md

Experimental CSV files are stored in the results/ directory.

Testing and Limitations

The FutureNet QoS queue-disc test suite passed during validation. The full ns-3 test run also reported two failures in unrelated Wi-Fi hidden-station examples. These failures are documented separately and should not be interpreted as FutureNet QoS test failures.

The experimental results are specific to the selected simulation parameters. Further experiments with different traffic loads, deadline distributions, queue limits, and WRR weights are needed to evaluate broader performance and fairness behavior.

Project Objective

The objective of FutureNet QoS (M05) is to investigate how packet priority and deadlines influence queue scheduling, deadline handling, fairness, and quality of service in ns-3 network simulations.

Repository

https://github.com/karthikeyagiramoni/futurenet-qos