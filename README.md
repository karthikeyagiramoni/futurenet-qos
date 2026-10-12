FutureNet QoS — M05

Deadline/Priority-Aware Queueing for ns-3.46.1

Overview

futurenet-qos is a custom ns-3 module for priority-aware packet scheduling and deadline-aware queue management. It provides configurable scheduling policies, packet metadata, deadline-miss handling, queue-limit enforcement, and tracing.

Features
Priority-aware scheduling: Supports priority levels 0–7, where 0 is the highest priority.
Scheduling modes:
0 — Strict Priority
1 — Earliest Deadline First (EDF)
2 — Hybrid scheduling
3 — Weighted Round Robin (WRR)
Deadline handling: Detects missed deadlines and supports dropping or transmitting expired packets.
Queue-limit enforcement: Configurable maximum number of queued packets.
Packet metadata: Traffic tags carry priority, flow ID, and importance.
Tracing: Supports dequeue, deadline-miss, and expired-packet-drop events.
Testing: Includes queue-disc tests for supported scheduling and queue-management behavior.
Requirements
ns-3.46.1
A working C++ build environment supported by ns-3
CMake and the build tools required by your ns-3 installation
Integration

Place the futurenet-qos module inside the ns-3 contrib/ directory:

ns-3.46.1/
└── contrib/
    └── futurenet-qos/
        ├── CMakeLists.txt
        ├── model/
        ├── test/
        ├── examples/
        └── README.md

The examples/ directory is optional if you have not added the example program to your repository yet.

Build

From the root of the ns-3.46.1 source tree, run:

./ns3 configure
./ns3 build

To build the example, if it is included and registered with the module:

./ns3 build futurenet-qos-example
Run the example

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

Deadline behavior can also be tested with the example's expired-packet policy and dequeue-time options, if those options are enabled in the version you are using.

Run tests

From the ns-3 source tree:

./test.py -s futurenet-queue-disc
Project objective

The objective of M05 is to explore how priority and packet deadlines can influence queue scheduling, packet expiration, and traffic quality of service in network simulations.

Repository

https://github.com/karthikeyagiramoni/futurenet-qos
