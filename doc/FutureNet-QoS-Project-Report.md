# Performance Evaluation of the FutureNet QoS Scheduling Module in ns-3

## Abstract

This project implements and evaluates a FutureNet Quality of Service (QoS) scheduling module for the ns-3 network simulator. The module supports four scheduling policies: Strict Priority, Earliest Deadline First (EDF), Hybrid scheduling, and Weighted Round Robin (WRR). It also incorporates packet deadline handling, queue-capacity enforcement, deadline-miss monitoring, and queue-delay measurements.

The evaluation compares the four scheduling modes under congested and lower-load traffic conditions. A separate controlled experiment evaluates WRR fairness using weights of 4:3:2:1. In the congested workload, EDF achieves the highest measured throughput (214.4 kbps) and the fewest deadline misses (10). Strict Priority achieves the lowest measured average queue delay (7.929 ms). The controlled WRR experiment produces service shares close to the configured weights, supporting the correctness of the weighted scheduling behavior for that workload.

These results demonstrate that scheduling policy affects performance under congestion, while differences become negligible when offered traffic does not create meaningful competition for service.

## 1. Introduction

Modern network applications have different QoS requirements. Some prioritize low latency, some require packets to be delivered before deadlines, and others need predictable service allocation among traffic classes. A single scheduling policy may not provide the best performance for all these requirements.

The FutureNet QoS module extends ns-3 with multiple scheduling policies and deadline-aware packet handling. The objective of this evaluation is to examine the behavior of the implemented policies under controlled simulation conditions and assess their effects on throughput, queue delay, packet drops, deadline misses, and per-priority service distribution.

### 1.1 Objectives

The objectives of this project are:

1. Implement and test four scheduling modes.
2. Evaluate deadline-aware scheduling and expired-packet handling.
3. Compare performance under congested and lower-load conditions.
4. Verify WRR service allocation against configured weights.
5. Collect quantitative results that can support future QoS research.

## 2. System Design and Scheduling Policies

The module uses a queue-disc architecture and supports four scheduling modes.

### 2.1 Strict Priority

Strict Priority serves the highest-priority nonempty class first. Within each class, packets follow FIFO ordering. This policy can provide low delay to high-priority traffic, but lower-priority traffic may experience longer delays when higher-priority classes remain busy.

### 2.2 Earliest Deadline First (EDF)

EDF selects the queued packet with the earliest deadline. The implementation uses an EDF internal queue to maintain deadline order. When packets have equal deadlines, FIFO ordering is tested.

EDF is designed to favor packets with imminent deadlines, although actual deadline satisfaction also depends on traffic load, service capacity, and deadline assignment.

### 2.3 Hybrid Scheduling

Hybrid scheduling combines class-based priority with EDF within each class. The scheduler considers higher-priority classes first and uses EDF to select packets within a class.

This policy aims to preserve class priority while accounting for packet deadlines within each class.

### 2.4 Weighted Round Robin (WRR)

WRR allocates service opportunities according to configurable class weights. The controlled fairness experiment uses weights of 4:3:2:1, corresponding to target service shares of 40%, 30%, 20%, and 10%.

WRR provides weighted service allocation; it does not, by itself, guarantee that every packet meets its deadline.

### 2.5 Deadline Handling and Monitoring

The module supports deadline checking, expired-packet policies, queue-capacity enforcement, and trace sources for deadline misses, expired drops, and class-dequeue delay.

Two expired-packet policies are available:

- **Policy 0:** Drop expired packets.
- **Policy 1:** Transmit expired packets.

The experiments primarily use policy 0. The existing `DeadlineTag` implementation was retained unchanged.

## 3. Experimental Methodology

All experiments were conducted using the FutureNet QoS example in ns-3.46.1.

### 3.1 Congested-workload configuration

| Parameter | Value |
|---|---:|
| Packet count | 100 |
| Queue limit | 20 packets |
| Arrival interval | 1 ms |
| Service interval | 2 ms |
| Experiment duration | 250 ms |
| Expired-packet policy | Drop (0) |
| Packet size | 100 bytes |
| Scheduling modes | 0, 1, 2, 3 |

The scheduling-mode mapping is:

| Mode | Policy |
|---:|---|
| 0 | Strict Priority |
| 1 | EDF |
| 2 | Hybrid |
| 3 | WRR |

The same traffic parameters were used for all four modes. Only the scheduling mode and output CSV filename changed.

### 3.2 Lower-load experiments

Two additional comparisons used the following settings:

| Parameter | Experiment A | Experiment B |
|---|---:|---:|
| Packet count limit | 100 | 100 |
| Queue limit | 20 | 20 |
| Arrival interval | 2 ms | 3 ms |
| Service interval | 2 ms | 2 ms |
| Duration | 250 ms | 250 ms |
| Expired-packet policy | Drop | Drop |

These experiments investigated whether the scheduling policy changed the results when traffic caused little queueing competition.

### 3.3 WRR fairness experiment

A separate controlled WRR run used:

| Parameter | Value |
|---|---:|
| Scheduling mode | WRR (3) |
| WRR weights | 4:3:2:1 |
| Packet count | 10,000 |
| Queue limit | 20,000 packets |
| Arrival interval | 1 ms |
| Service interval | 2 ms |
| Duration | 10,000 ms |
| Fairness-test option | Enabled |
| Expired-packet policy | Drop (0) |

The fairness-test option assigns long deadlines to reduce deadline expiry as a confounding factor. The large queue limit allows the simulation to maintain a backlog during the experiment.

### 3.4 Performance metrics

The CSV output records:

- Generated and enqueued packets
- Enqueue drops and expired drops
- Transmitted packets and packets remaining in the queue
- Deadline misses
- Average queue delay
- Throughput
- Transmitted packet counts by priority class

Throughput is reported in bits per second. Average queue delay is calculated from recorded enqueue timestamps for packets that are successfully dequeued and transmitted, according to the example's tracing logic.

## 4. Results and Analysis

### 4.1 Congested-workload comparison

The following values were obtained from the four simulation CSV files.

| Metric | Strict Priority | EDF | Hybrid | WRR |
|---|---:|---:|---:|---:|
| Generated packets | 100 | 100 | 100 | 100 |
| Enqueued packets | 79 | 77 | 79 | 84 |
| Enqueue drops | 21 | 23 | 21 | 16 |
| Expired drops | 23 | 10 | 21 | 25 |
| Transmitted packets | 56 | 67 | 58 | 59 |
| Remaining packets | 0 | 0 | 0 | 0 |
| Deadline misses | 23 | 10 | 21 | 25 |
| Average queue delay (ms) | **7.929** | 23.060 | 8.776 | 12.136 |
| Throughput (bps) | 179,200 | **214,400** | 185,600 | 188,800 |

#### Observations

**EDF:** EDF transmitted 67 packets, the highest total among the four modes. It achieved 214.4 kbps and recorded 10 deadline misses, the lowest count in this experiment. Its average queue delay was nevertheless the highest at 23.060 ms. This illustrates that throughput, deadline misses, and average delay measure different aspects of performance.

**Strict Priority:** Strict Priority achieved the lowest average queue delay, 7.929 ms. It transmitted 56 packets and recorded 23 deadline misses. Its behavior reflects the emphasis on class priority rather than global deadline ordering.

**Hybrid:** Hybrid transmitted 58 packets at 185.6 kbps and recorded 21 deadline misses. Its average queue delay was 8.776 ms. In this workload, its overall results were between Strict Priority and EDF for throughput and deadline misses.

**WRR:** WRR transmitted 59 packets at 188.8 kbps. It had the fewest enqueue drops (16), but the highest number of expired drops (25) and deadline misses (25). Weighted service allocation did not prevent deadline misses under this workload.

These findings apply to the specific traffic generation, deadlines, queue limit, and service timing used here. They should not be interpreted as universal rankings of the algorithms.

### 4.2 Per-priority transmission counts under congestion

| Scheduling mode | Priority 0 | Priority 1 | Priority 2 | Priority 3 |
|---|---:|---:|---:|---:|
| Strict Priority | 25 | 15 | 15 | 1 |
| EDF | 25 | 8 | 25 | 9 |
| Hybrid | 25 | 17 | 16 | 0 |
| WRR | 25 | 15 | 14 | 5 |

Strict Priority and Hybrid do not guarantee equal service among classes. EDF selects packets according to deadlines, so its class distribution depends on the deadlines of packets that are queued and eligible for transmission. The congested WRR counts also need not match the configured weights exactly because the finite run includes class-specific packet arrivals, queue occupancy, and deadline drops.

### 4.3 Lower-load comparison: 2 ms arrival interval

All four scheduling modes produced identical results:

| Metric | Result for each mode |
|---|---:|
| Generated packets | 100 |
| Enqueued packets | 100 |
| Enqueue drops | 0 |
| Expired drops | 0 |
| Transmitted packets | 100 |
| Remaining packets | 0 |
| Deadline misses | 0 |
| Average queue delay | 0.000 ms |
| Throughput | 320,000 bps |

The arrival interval matches the service interval. The recorded zero average queue delay is consistent with packets being available for service at the same simulation timestamps at which they arrive. This result depends on the event timing and configuration of the example; it does not imply that queueing delay will always be zero when the average arrival and service rates match.

Because all packets were transmitted without drops or deadline misses, there was little opportunity for the scheduling policies to produce different results.

### 4.4 Lower-load comparison: 3 ms arrival interval

All four modes again produced identical results:

| Metric | Result for each mode |
|---|---:|
| Generated packets | 84 |
| Enqueued packets | 84 |
| Enqueue drops | 0 |
| Expired drops | 0 |
| Transmitted packets | 83 |
| Remaining packets | 1 |
| Deadline misses | 0 |
| Average queue delay | 0.494 ms |
| Throughput | 265,600 bps |

The experiment generated 84 packets during the configured duration, transmitted 83, and left one packet in the queue at the end of the run. Since arrivals were slower than service opportunities, the queue remained lightly loaded and all scheduling modes produced the same recorded performance.

The results demonstrate why scheduler comparisons should include congested workloads: under light load, several policies may produce identical outcomes.

### 4.5 WRR fairness evaluation

The controlled fairness run produced the following results:

| Metric | Measured result |
|---|---:|
| Generated packets | 10,000 |
| Enqueued packets | 10,000 |
| Enqueue drops | 0 |
| Expired drops | 0 |
| Transmitted packets | 5,000 |
| Remaining packets | 5,000 |
| Deadline misses | 0 |
| Average queue delay | 2,000.399 ms |
| Throughput | 400,000 bps |

The per-priority results were:

| Priority | Configured weight | Expected share | Transmitted packets | Measured share |
|---|---:|---:|---:|---:|
| Priority 0 | 4 | 40% | 1,999 | 39.98% |
| Priority 1 | 3 | 30% | 1,501 | 30.02% |
| Priority 2 | 2 | 20% | 1,000 | 20.00% |
| Priority 3 | 1 | 10% | 500 | 10.00% |
| **Total** | **10** | **100%** | **5,000** | **100%** |

The measured service shares are extremely close to the expected 40:30:20:10 distribution. The largest absolute deviation is 0.02 percentage points.

This result supports the conclusion that the implemented WRR scheduler distributes service opportunities according to the configured weights in this controlled experiment.

However, 5,000 of the 10,000 generated packets remained queued at the end of the simulation. The average queue delay of approximately 2 seconds indicates that the offered load exceeded the service rate. The fairness experiment validates the service distribution under backlog; it does not demonstrate that the configuration is stable under that traffic load or that it meets latency requirements.

## 5. Packet Accounting and Result Consistency

The recorded results satisfy the following packet-accounting relationship:

**Generated = Enqueue drops + Expired drops + Transmitted + Remaining in queue**

For the congested EDF run:

100 = 23 + 10 + 67 + 0

For the controlled WRR fairness run:

10,000 = 0 + 0 + 5,000 + 5,000

The per-priority transmission counts also sum to the total transmitted count in each reported run. These checks provide evidence that the example's packet counters are internally consistent for the recorded experiments.

Deadline misses and expired drops are related but distinct metrics: a deadline miss indicates that a packet was detected as late, while an expired drop indicates that the configured drop policy discarded it. Under a transmit-expired policy, a late packet may be transmitted instead of dropped.

## 6. Testing and Validation Status

The following validation activities have been completed:

1. The `futurenet-qos` queue-disc test suite passed.
2. The full ns-3 test run was attempted.
3. The full run reported failures in two unrelated Wi-Fi example tests:
   - `wifi-simple-ht-hidden-stations` with RTS disabled.
   - `wifi-simple-ht-hidden-stations` with RTS enabled.
4. The four scheduling modes were compared under congested and lower-load conditions.
5. The WRR fairness experiment was completed and its measured service shares were checked.
6. The Git repository was clean of tracked changes, with commit `41bf2ec` synchronized with `origin/main`. An untracked local backup file remained.

The two Wi-Fi example failures have not been diagnosed further. They should be reported transparently as failures observed during the full ns-3 test run; the available evidence does not establish their root cause. They are not tests of the FutureNet QoS module, but their failure means the full simulator test suite should not be described as passing completely.

## 7. Limitations

Several limitations should be considered when interpreting these results.

**Limited traffic scenarios:** The evaluation uses a small set of fixed arrival intervals, service intervals, queue limits, and simulation durations. Additional configurations are needed to generalize the findings.

**Finite simulation duration:** Each experiment covers a limited period. Results can be affected by simulation start-up, end-of-run queue occupancy, and the timing of arrivals and service events.

**Single configured traffic pattern:** The example uses a repeating priority pattern and generated deadlines. Real networks may have different traffic distributions, burstiness, packet sizes, and deadline requirements.

**No statistical repetitions:** The reported results come from individual simulation runs. Repeated runs with varied seeds or traffic patterns would be necessary to characterize variability where randomness is involved.

**Trade-offs between metrics:** The mode with the highest throughput does not necessarily have the lowest delay. EDF had the highest throughput and fewest deadline misses in the congested workload, while Strict Priority had the lowest average queue delay. Policy selection depends on the application's QoS objective.

**WRR backlog:** The fairness experiment maintained a substantial queue backlog. The measured weight distribution validates relative service allocation, but the high queue delay shows that WRR does not compensate for an offered load above service capacity.

**Full ns-3 test-suite failures:** Two Wi-Fi example tests failed in the full test run. Their cause remains unverified, so complete ns-3 regression validation is still outstanding.

## 8. Future Work

Future evaluation could strengthen the module and its performance claims by:

1. Repeating experiments over multiple random seeds and reporting averages and variability.
2. Sweeping arrival rates and queue limits to identify congestion thresholds.
3. Testing several WRR weight configurations and verifying service shares under different traffic mixes.
4. Measuring per-priority delay and deadline-miss rates rather than only aggregate metrics.
5. Comparing deadline-drop and transmit-expired policies under identical traffic.
6. Testing bursty traffic, different packet sizes, and more varied deadline distributions.
7. Investigating the two Wi-Fi example failures separately to establish whether they are environmental, configuration-related, or reproducible regressions.
8. Documenting the exact ns-3 revision, build configuration, and commands used for each reported experiment.

## 9. Conclusion

The FutureNet QoS module provides four scheduling policies and associated deadline-handling and monitoring functionality. In the congested experiment, EDF achieved the highest throughput at 214.4 kbps and the lowest deadline-miss count at 10, while Strict Priority achieved the lowest average queue delay at 7.929 ms. Hybrid combined class priority with EDF ordering within classes, and WRR provided weighted service allocation.

Under lower-load conditions, all four policies produced identical results, indicating that the scheduling choice had little observable effect when packets did not meaningfully compete for service. In the controlled WRR fairness experiment, measured service shares of 39.98%, 30.02%, 20%, and 10% closely matched the configured weights of 4:3:2:1.

The results support the functional behavior of the scheduling policies for the tested scenarios. Broader performance claims require more extensive experiments, and complete ns-3 regression validation remains pending because two unrelated Wi-Fi example tests failed during the full test run.

Overall, the evaluation provides a useful baseline for further development and systematic testing of FutureNet QoS scheduling in ns-3.


