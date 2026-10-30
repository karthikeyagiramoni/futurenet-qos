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

The basic `futurenet-qos-example` writes one summary row per run to
`futurenet-qos-results.csv` (or the path passed with `--csvFile`). It appends
rows, writing the header if the file is new or empty. Its columns are:

| Column | Meaning |
|---|---|
| `mode` | Scheduling mode: `0` Strict Priority, `1` EDF, `2` Hybrid, `3` WRR. |
| `generated` | Packets generated during the simulation. The run can stop generating at `durationMs` before reaching `packetCount`. |
| `enqueued` | Generated packets accepted into the queue. |
| `enqueue_drops` | Packets rejected when enqueued, for example because the queue limit was reached. |
| `expired_drops` | Packets removed because their deadlines expired when `expiredPolicy=0`. |
| `transmitted` | Packets dequeued and transmitted, including expired packets when `expiredPolicy=1`. |
| `remaining_in_queue` | Packets still queued at the end of the simulation. |
| `deadline_misses` | Packets found past their deadline when considered for dequeue; includes expired packets transmitted if `expiredPolicy=1`. |
| `avg_queue_delay_ms` | Mean queue sojourn time for transmitted packets, in milliseconds. |
| `throughput_bps` | Transmitted packet bytes converted to bits and divided by the configured simulation duration. |
| `priority_0_tx` … `priority_7_tx` | Transmitted packet counts for each priority. Classes above `maxPriority` have zero transmissions. |

Packet accounting can be checked as
`generated = enqueue_drops + expired_drops + transmitted + remaining_in_queue`.
The comparison and WRR result files record these packet counts, deadline
metrics, delay, throughput, and priority transmission counts. The comparison
experiments generate priorities 0 through 3, matching the example's default.
The WRR fairness experiment uses weights 4:3:2:1 across those priorities;
priorities 4 through 7 are unused.

### Basic Example Scheduling Comparison

The following `futurenet-qos-example` results are from the congested workload
in `congested-comparison.csv` (1 ms packet arrival interval and 2 ms service
interval):

| Mode | Policy | Transmitted | Enqueue drops | Expired drops | Remaining | Throughput (bps) |
|---:|---|---:|---:|---:|---:|---:|
| 0 | Strict Priority | 56 | 21 | 23 | 0 | 179,200 |
| 1 | EDF | 67 | 23 | 10 | 0 | 214,400 |
| 2 | Hybrid | 58 | 21 | 21 | 0 | 185,600 |
| 3 | WRR | 59 | 16 | 25 | 0 | 188,800 |

The advanced multi-flow CSV records one row per flow and scheduling mode,
including the mode name, each flow's priority, packet size, arrival interval,
deadline range, packet counters, average queue delay, and throughput. It was
generated with the default advanced-example workload: a 100-packet shared
queue, 2 ms service interval, 1,000 ms duration, and expired-packet drop
policy. The workload offers 250, 200, 167, and 143 packets per second for
priorities 0 through 3, respectively, and assigns per-packet deadlines from
250 to 1,000 ms after arrival, independently of priority. It starts with an
eight-packet-per-flow burst so all four scheduling classes are backlogged.

| Mode | Policy | Transmitted | Enqueue drops | Expired drops | Remaining | Throughput (bps) |
|---:|---|---:|---:|---:|---:|---:|
| 0 | Strict Priority | 500 | 159 | 33 | 96 | 1,537,600 |
| 1 | EDF | 500 | 190 | 0 | 98 | 2,216,800 |
| 2 | Hybrid | 500 | 142 | 48 | 98 | 1,407,200 |
| 3 | WRR | 500 | 175 | 15 | 98 | 1,722,400 |

These are workload-specific measurements, not general scheduler rankings.
Strict Priority favors lower-numbered priorities, while EDF selects packets
across priorities by deadline. Hybrid preserves strict priority between
classes and uses EDF ordering within each class; WRR uses weights 4:3:2:1.
Throughput differs because the flows use different packet sizes. Each example
run also writes a packet-level transmission-order CSV; use it to inspect the
priorities and deadlines actually selected by that run.

## Interpretation notes

Results depend on the selected workload and simulation parameters.
The 2 ms and 3 ms experiments are lower-load comparisons; the congested
experiment demonstrates different scheduling behavior under contention.
The WRR fairness run measures service shares while packets remain queued.

These CSV files preserve the measured values summarized in the project
report. Refer to `doc/FutureNet-QoS-Project-Report.md` for methodology,
analysis, validation status, and limitations.
