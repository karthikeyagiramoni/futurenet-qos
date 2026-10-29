# FutureNet QoS Experimental Results

This directory contains measured CSV results from the FutureNet QoS
scheduling experiments in ns-3.

## Files

- `congested-comparison.csv`: Four scheduling modes under a congested workload.
- `low-load-2ms.csv`: Scheduling comparison with a 2 ms arrival interval.
- `low-load-3ms.csv`: Scheduling comparison with a 3 ms arrival interval.
- `wrr-fairness.csv`: WRR service-share experiment using weights 4:3:2:1.
- `advanced-multiflow.csv`: Per-flow results for four simultaneous flows under
  each of the four scheduling modes.

## Scheduling modes

- 0: Strict Priority
- 1: Earliest Deadline First (EDF)
- 2: Hybrid
- 3: Weighted Round Robin (WRR)

## CSV metrics

The comparison and WRR files record packet counts, enqueue and deadline drops,
transmissions, remaining queue occupancy, deadline misses, average queue
delay, throughput, and transmissions by priority class. These experiments
generate priorities 0 through 3, matching the example's default. The WRR
fairness experiment uses weights 4:3:2:1 across those priorities. Priorities
4 through 7 are unused in these experiments.

The advanced multi-flow CSV records one row per flow and scheduling mode,
including each flow's priority, packet size, arrival interval, deadline,
packet counters, average queue delay, and throughput. It was generated with
the default advanced-example workload: a 100-packet shared queue, 2 ms service
interval, 1,000 ms duration, and expired-packet drop policy.

| Mode | Policy | Transmitted | Enqueue drops | Expired drops | Remaining | Throughput (bps) |
|---:|---|---:|---:|---:|---:|---:|
| 0 | Strict Priority | 500 | 1,147 | 337 | 100 | 400,000 |
| 1 | EDF | 500 | 0 | 1,511 | 73 | 3,058,400 |
| 2 | Hybrid | 500 | 1,147 | 337 | 100 | 400,000 |
| 3 | WRR | 500 | 656 | 914 | 14 | 2,703,200 |

These are workload-specific measurements, not general scheduler rankings.
Strict Priority and Hybrid served only priority 0 in this congested setup;
EDF and WRR transmitted packets from multiple flows. Throughput differs
because the flows use different packet sizes.

## Interpretation notes

Results depend on the selected workload and simulation parameters.
The 2 ms and 3 ms experiments are lower-load comparisons; the congested
experiment demonstrates different scheduling behavior under contention.
The WRR fairness run measures service shares while packets remain queued.

These CSV files preserve the measured values summarized in the project
report. Refer to `doc/FutureNet-QoS-Project-Report.md` for methodology,
analysis, validation status, and limitations.
