# Lab 8 – Task Scheduling Project (CS 375)

Ten CPU scheduling algorithms in C++, first as standalone lab programs (Part 2) and then
combined into one modular command-line simulator (Part 3).

**Author:** Santiago Soto  
**Report:** [report/report.pdf](report/report.pdf) – implementations, enhancements, test output and analysis

## Quick start

```bash
make                                          # builds everything into build/
./build/fcfs                                  # run a lab exercise
./build/simulator --scheduler rr --quantum 4  # run the simulator
./build/simulator --help                      # list every option
```

Requires `g++` (C++17) and `make`. The report additionally needs `pdflatex`.

## What is in this repository

```
submission/                  <- everything that gets submitted
  Part2_Lab/                 <- Part 2: the 10 .cpp files, one per exercise
    fcfs.cpp  sjf.cpp  srtf.cpp  priority.cpp  rr.cpp
    mlq.cpp   mlfq.cpp lottery.cpp  cfs.cpp    edf.cpp
  Part3_Simulator/           <- Part 3: the simulator
    simulator.cpp            main(), argument parsing, reports
    process.h                Process struct
    scheduler.h              abstract Scheduler base class
    schedulers.h             the 10 derived scheduler classes
    utils.h                  file loading, task generation, metrics, printing
    tasks.txt                sample input (12 tasks)
    tasks_deadlines.txt      sample input with deadlines (10 tasks)
report/
  report.pdf                 the written report
  report.tex                 its LaTeX source
  screenshots/               the 20 terminal screenshots used in the report
Makefile
```

## What to submit

| Where | What |
|---|---|
| Part 2 (Canvas) | the 10 files in `submission/Part2_Lab/` + the screenshots `ex01`–`ex10` |
| Part 3 (Canvas, no zip) | every file in `submission/Part3_Simulator/` + `report/report.pdf` + the GitHub link |

`report.pdf` contains all the screenshots, so it covers the screenshot requirement for both parts.

## Part 2 – lab exercises

Each file is self-contained (it includes the common code from Part 1 of the handout) and
compiles on its own, e.g. `g++ fcfs.cpp -o fcfs`. Each prints the process table, a Gantt
chart, per-process waiting/turnaround times and the averages.

| # | File | Algorithm | Enhancement |
|---|---|---|---|
| 1 | `fcfs.cpp` | First-Come, First-Served | – |
| 2 | `sjf.cpp` | Shortest Job First (non-preemptive) | – |
| 3 | `srtf.cpp` | Shortest Remaining Time First | preemption log |
| 4 | `priority.cpp` | Priority (non-preemptive) | aging: +1 priority level per 5 units waited |
| 5 | `rr.cpp` | Round Robin, quantum 4 | – |
| 6 | `mlq.cpp` | Multilevel Queue (high: RR, low: FCFS) | high arrivals preempt the low queue |
| 7 | `mlfq.cpp` | Multilevel Feedback Queue (quanta 2/4/8) | promotion after waiting 8 units |
| 8 | `lottery.cpp` | Lottery, tickets = 10 / priority | losers gain a ticket each draw |
| 9 | `cfs.cpp` | Simplified CFS (min-heap on vruntime) | weights from priority |
| 10 | `edf.cpp` | Earliest Deadline First (preemptive) | schedulability check, missed-deadline report |

`lottery` is random; pass a seed to repeat a run: `./build/lottery 7`.

Results on the assignment table (P1–P4):

| Scheduler | Avg waiting | Avg turnaround |
|---|---|---|
| FCFS | 8.75 | 15.25 |
| SJF | 7.75 | 14.25 |
| SRTF | 6.50 | 13.00 |
| Priority + aging | 8.75 | 15.25 |
| Round Robin | 11.75 | 18.25 |
| MLQ | 8.75 | 15.25 |
| MLFQ | 14.00 | 20.50 |
| CFS | 12.25 | 18.75 |
| EDF | 6.50 | 13.00 |

## Part 3 – the simulator

### Build and run

```bash
make sim
# or, inside submission/Part3_Simulator/:
g++ -std=c++17 -pthread simulator.cpp -o simulator
```

```bash
./build/simulator --scheduler rr --quantum 4
./build/simulator --scheduler srtf --input submission/Part3_Simulator/tasks.txt --output log.txt
./build/simulator --scheduler rr --random --num 20
./build/simulator --compare --random --num 20 --seed 42
./build/simulator --compare rr,srtf --input submission/Part3_Simulator/tasks.txt
```

### Options

| Option | Meaning | Default |
|---|---|---|
| `--scheduler <name>` | `fcfs` `sjf` `srtf` `priority` `rr` `mlq` `mlfq` `lottery` `cfs` `edf` | – |
| `--compare [a,b,...]` | run several schedulers on the same tasks and print one table | all 10 |
| `--input <file>` | read tasks from a file | assignment table |
| `--random` | generate random tasks | off |
| `--web` | generate a web-server workload (requests, mostly short) | off |
| `--num <n>` | number of tasks for `--random` / `--web` | 10 |
| `--seed <n>` | seed for random tasks, lottery draws and I/O, to repeat a run | time-based |
| `--quantum <n>` | time quantum for `rr`, `mlq`, `lottery` | 4 |
| `--aging <n>` | `priority`: gain one level per n units waited (0 = off) | 5 |
| `--io <percent>` | `rr`: chance a process blocks for I/O after a time slice | 0 |
| `--cpus <n>` | number of CPUs (load-balanced, one thread each) | 1 |
| `--output <file>` | also write the results to a file | – |
| `--verbose` | print every scheduler event (default: first 15) | off |
| `--help` | show usage | |

### Input file format

One task per line; `#` starts a comment. The deadline is optional and absolute.

```
# ID arrival burst priority [deadline]
T1  0  10  3
T2  1   2  1
S1  0   3  2  8
```

Lower priority number = higher priority (1 is the highest). EDF gives tasks without a
deadline `arrival + 2 × burst`. Invalid lines are reported with their line number and skipped.

### Output

Gantt chart as `ID(duration)` plus a timeline, a per-task table (start, finish, waiting,
turnaround, response), then average waiting / turnaround / response time, CPU utilization,
throughput and context switches. Schedulers also list notable events (aging decisions,
MLFQ promotions and demotions, I/O blocks, missed deadlines).

### Design

`Scheduler` (in `scheduler.h`) is an abstract base class with a pure virtual
`schedule(processes, gantt, total_time)`. Each algorithm in `schedulers.h` is a derived
class overriding it. `main()` picks one through the `makeScheduler()` factory and uses it
only through a `std::unique_ptr<Scheduler>`. To add a scheduler: write one class, add one
line to `makeScheduler()`, and add its name to `kSchedulers`.

With `--cpus n`, a load balancer assigns each task to the CPU that will be free soonest;
each CPU then runs its own scheduler instance in its own `std::thread`.

### Comparison

All ten schedulers on `tasks.txt` (12 tasks mixing long and short jobs), from
`./build/simulator --compare --input submission/Part3_Simulator/tasks.txt --seed 42`:

| Scheduler | Avg waiting | Avg turnaround | Avg response | Context switches |
|---|---|---|---|---|
| fcfs | 22.67 | 27.58 | 22.67 | 11 |
| sjf | 12.50 | 17.42 | 12.50 | 11 |
| srtf | 9.92 | 14.83 | 7.08 | 14 |
| priority | 19.83 | 24.75 | 19.83 | 11 |
| rr | 22.17 | 27.08 | 12.08 | 19 |
| mlq | 13.00 | 17.92 | 12.08 | 15 |
| mlfq | 20.25 | 25.17 | 3.25 | 29 |
| lottery | 21.08 | 26.00 | 11.67 | 19 |
| cfs | 18.17 | 23.08 | 2.25 | 30 |
| edf | 10.75 | 15.67 | 8.75 | 14 |

- **SRTF** has the lowest average waiting time, but it needs burst lengths in advance and can starve long jobs.
- **MLFQ and CFS** respond fastest (best for interactive / I/O-bound work) at the cost of the most context switches.
- **FCFS and SJF** switch least (fine for CPU-bound batch work); FCFS suffers when short tasks queue behind long ones.
- **Round Robin** is the simple middle ground: no burst estimates needed and a bounded wait for the next turn.

The full analysis is in the report.

## The report

[report/report.pdf](report/report.pdf) is built from `report/report.tex` and the screenshots
in `report/screenshots/`. Each screenshot is the output of one command, run from the
repository root after `make`:

| File name | Command |
   |---|---|
| `ex01_fcfs` | `./build/fcfs` |
| `ex02_sjf` | `./build/sjf` |
| `ex03_srtf` | `./build/srtf` |
| `ex04_priority` | `./build/priority` |
| `ex05_rr` | `./build/rr` |
| `ex06_mlq` | `./build/mlq` |
| `ex07_mlfq` | `./build/mlfq` |
| `ex08_lottery` | `./build/lottery 7` |
| `ex09_cfs` | `./build/cfs` |
| `ex10_edf` | `./build/edf` |
| `p3_01_rr_default` | `./build/simulator --scheduler rr --quantum 4` |
| `p3_02_rr_random` | `./build/simulator --scheduler rr --random --seed 42` |
| `p3_03_srtf_file` | `./build/simulator --scheduler srtf --input submission/Part3_Simulator/tasks.txt` |
| `p3_04_compare` | `./build/simulator --compare --input submission/Part3_Simulator/tasks.txt --seed 42` |
| `p3_05_priority_aging` | `./build/simulator --scheduler priority --aging 3 --input submission/Part3_Simulator/tasks.txt` |
| `p3_06_edf_deadlines` | `./build/simulator --scheduler edf --input submission/Part3_Simulator/tasks_deadlines.txt` |
| `p3_07_rr_io` | `./build/simulator --scheduler rr --io 40 --seed 7 --input submission/Part3_Simulator/tasks.txt` |
| `p3_08_web_cpus` | `./build/simulator --scheduler rr --web --num 12 --cpus 2 --seed 3` |
| `p3_09_output_file` | `./build/simulator --scheduler mlfq --output build/log.txt; ls -l build/log.txt` |
| `p3_10_errors` | `./build/simulator --scheduler foo; ./build/simulator --scheduler rr --input missing.txt; ./build/simulator --scheduler rr --quantum abc` |

To rebuild the PDF after changing the text or a screenshot, run `make report`. The report
picks each image up by file name (`.png` or `.jpg`); a missing one shows as a box naming
the file it expects.

## References

- Lab handout: *Lab 8 – Implementing and Enhancing Task Schedulers in C++* (starter code).
- Silberschatz, Galvin, Gagne, *Operating System Concepts*, 10th ed., ch. 5.
- Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, ch. 7–9. https://pages.cs.wisc.edu/~remzi/OSTEP/
- Waldspurger & Weihl, "Lottery Scheduling: Flexible Proportional-Share Resource Management", OSDI 1994.
- Liu & Layland, "Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment", JACM 1973.
