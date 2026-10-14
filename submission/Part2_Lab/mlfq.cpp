// mlfq.cpp - Exercise 7: Multilevel Feedback Queue Scheduling
// Compile: g++ mlfq.cpp -o mlfq
#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include <deque>
// ---------------------------------------------------------------------------
// Common code (Part 1)
// ---------------------------------------------------------------------------
struct Process {
    std::string id;
    int arrival_time;
    int burst_time;
    int priority;  // Lower number = higher priority
    int remaining_time;  // For preemptive
    int waiting_time;
    int turnaround_time;
};

typedef std::vector<std::pair<std::string, int>> Gantt;

// The process table from the assignment
std::vector<Process> defaultProcesses() {
    return {
        {"P1", 0, 8, 2, 8, 0, 0},
        {"P2", 1, 4, 1, 4, 0, 0},
        {"P3", 2, 9, 3, 9, 0, 0},
        {"P4", 3, 5, 4, 5, 0, 0}
    };
}

void calculateMetrics(std::vector<Process>& processes, int total_time) {
    double avg_wait = 0, avg_turn = 0, busy_time = 0;
    for (auto& p : processes) {
        avg_wait += p.waiting_time;
        avg_turn += p.turnaround_time;
        busy_time += p.burst_time;
    }
    avg_wait /= processes.size();
    avg_turn /= processes.size();
    double cpu_util = total_time > 0 ? busy_time / total_time * 100 : 0;  // Busy time / elapsed time, so idle time counts
    std::cout << "Avg Waiting Time: " << avg_wait << "\n";
    std::cout << "Avg Turnaround Time: " << avg_turn << "\n";
    std::cout << "CPU Utilization: " << cpu_util << "%\n";
}

// Function to print text-based Gantt chart: each entry is ID(duration), then a timeline
void printGantt(const Gantt& gantt) {
    std::cout << "Gantt Chart: ";
    for (auto& entry : gantt) {
        std::cout << entry.first << "(" << entry.second << ") ";
    }
    std::cout << "\nTimeline:    0";
    int t = 0;
    for (auto& entry : gantt) {
        t += entry.second;
        std::cout << " [" << entry.first << "] " << t;
    }
    std::cout << "\n";
}

// Add a run to the Gantt chart, merging it with the previous entry if it is the same process
void addToGantt(Gantt& gantt, const std::string& id, int duration) {
    if (duration <= 0) return;
    if (!gantt.empty() && gantt.back().first == id) gantt.back().second += duration;
    else gantt.emplace_back(id, duration);
}

// Record the final times of a process that completed at current_time
void finish(Process& p, int current_time) {
    p.turnaround_time = current_time - p.arrival_time;
    p.waiting_time = p.turnaround_time - p.burst_time;
}

void printHeader(const std::string& title, const std::vector<Process>& processes) {
    std::cout << "=== " << title << " ===\n\nProcess table:\n";
    std::cout << std::left << std::setw(9) << "Process" << std::setw(9) << "Arrival" << std::setw(7) << "Burst"
              << "Priority\n";
    for (auto& p : processes)
        std::cout << std::left << std::setw(9) << p.id << std::setw(9) << p.arrival_time << std::setw(7)
                  << p.burst_time << p.priority << "\n";
    std::cout << "\n";
}

void printResults(std::vector<Process>& processes, const Gantt& gantt, int total_time) {
    printGantt(gantt);
    std::cout << "\n" << std::left << std::setw(9) << "Process" << std::setw(9) << "Waiting" << "Turnaround\n";
    for (auto& p : processes)
        std::cout << std::left << std::setw(9) << p.id << std::setw(9) << p.waiting_time << p.turnaround_time << "\n";
    std::cout << "\n";
    calculateMetrics(processes, total_time);
}
// ---------------------------------------------------------------------------

// Three Round Robin queues with quanta 2, 4, 8. Every process starts in the top
// queue. Feedback: a process that uses its full quantum is CPU-bound and is
// demoted one level. Enhancement: a process that has waited PROMOTE_AFTER units
// in a lower queue is promoted one level so it cannot starve.
const int PROMOTE_AFTER = 8;

int main() {
    std::vector<Process> processes = defaultProcesses();  // Already in arrival order
    printHeader("Exercise 7: Multilevel Feedback Queue (quanta 2, 4, 8)", processes);

    for (auto& p : processes) p.remaining_time = p.burst_time;
    const std::vector<int> quanta = {2, 4, 8};
    const int levels = quanta.size();
    std::vector<std::deque<size_t>> queues(levels);  // Hold indexes into processes
    std::vector<int> queued_at(processes.size(), 0);  // When each process last joined a queue
    Gantt gantt;
    int current_time = 0;
    size_t next = 0, completed = 0;

    auto enqueue = [&](size_t i, int level) {
        queues[level].push_back(i);
        queued_at[i] = current_time;
    };
    auto admit = [&]() {  // New arrivals start in the top queue
        while (next < processes.size() && processes[next].arrival_time <= current_time) enqueue(next++, 0);
    };

    std::cout << "Feedback log:\n";
    while (completed < processes.size()) {
        admit();

        // Enhancement: promote if waiting too long
        for (int l = 1; l < levels; ++l) {
            for (auto it = queues[l].begin(); it != queues[l].end();) {
                size_t i = *it;
                if (current_time - queued_at[i] >= PROMOTE_AFTER) {
                    it = queues[l].erase(it);
                    enqueue(i, l - 1);
                    std::cout << "t=" << current_time << ": " << processes[i].id << " promoted Q" << l << " -> Q"
                              << l - 1 << " (waited " << PROMOTE_AFTER << "+ units)\n";
                } else {
                    ++it;
                }
            }
        }

        // Prefer higher queues
        int level = 0;
        while (level < levels && queues[level].empty()) ++level;
        if (level == levels) {
            addToGantt(gantt, "IDLE", 1);
            current_time++;
            continue;
        }

        size_t i = queues[level].front();
        queues[level].pop_front();
        Process& p = processes[i];
        int used = 0;
        while (used < quanta[level] && p.remaining_time > 0) {
            addToGantt(gantt, p.id, 1);
            p.remaining_time--;
            current_time++;
            used++;
            admit();
            if (level > 0 && !queues[0].empty()) break;  // A new arrival in the top queue preempts
        }

        if (p.remaining_time == 0) {
            finish(p, current_time);
            completed++;
        } else if (used == quanta[level]) {  // Used the full quantum: move down
            int lower = std::min(level + 1, levels - 1);
            if (lower != level)
                std::cout << "t=" << current_time << ": " << p.id << " used its full quantum, demoted Q" << level
                          << " -> Q" << lower << "\n";
            enqueue(i, lower);
        } else {  // Preempted before the quantum ended: stays at its level
            enqueue(i, level);
        }
    }
    std::cout << "\n";

    printResults(processes, gantt, current_time);
    return 0;
}
