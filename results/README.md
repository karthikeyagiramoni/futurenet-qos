# FutureNet QoS Experimental Results

This directory contains measured CSV results from the FutureNet QoS
scheduling experiments in ns-3.

## Files

- `congested-comparison.csv`: Four scheduling modes under a congested workload.
- `low-load-2ms.csv`: Scheduling comparison with a 2 ms arrival interval.
- `low-load-3ms.csv`: Scheduling comparison with a 3 ms arrival interval.
- `wrr-fairness.csv`: WRR service-share experiment using weights 4:3:2:1.

## Scheduling modes

- 0: Strict Priority
- 1: Earliest Deadline First (EDF)
- 2: Hybrid
- 3: Weighted Round Robin (WRR)

## CSV metrics

The files record packet counts, enqueue and deadline drops, transmissions,
remaining queue occupancy, deadline misses, average queue delay,
throughput, and transmissions by priority class.

## Interpretation notes

Results depend on the selected workload and simulation parameters.
The 2 ms and 3 ms experiments are lower-load comparisons; the congested
experiment demonstrates different scheduling behavior under contention.
The WRR fairness run measures service shares while packets remain queued.

These CSV files preserve the measured values summarized in the project
report. Refer to `doc/FutureNet-QoS-Project-Report.md` for methodology,
analysis, validation status, and limitations.
