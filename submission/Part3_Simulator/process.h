// process.h - the task ("process") record shared by every scheduler
#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <utility>
#include <vector>

struct Process {
    std::string id;
    int arrival_time;
    int burst_time;
    int priority;  // Lower = higher priority (1 is the highest)
    int remaining_time;
    int waiting_time = 0;
    int turnaround_time = 0;
    int deadline = 0;          // Absolute deadline; 0 = none given (EDF derives one)
    int start_time = -1;       // First time on the CPU (-1 = not started yet)
    int completion_time = 0;
    int io_time = 0;           // Time spent blocked on simulated I/O
    double vruntime = 0;       // For CFS
    int cpu = 0;               // Which CPU ran it (multi-CPU mode)
};

// One Gantt entry = (process id or "IDLE", duration)
using Gantt = std::vector<std::pair<std::string, int>>;

#endif
