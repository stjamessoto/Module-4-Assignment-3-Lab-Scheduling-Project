// cfs.cpp - Exercise 9: Completely Fair Scheduler (simplified)
// Compile: g++ cfs.cpp -o cfs
#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include <cmath>
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
    double vruntime = 0;  // Virtual runtime (added for CFS)
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

// Simplified CFS: always run the process with the smallest virtual runtime.
// Enhancement - weights based on priority: weight = 1024 / 1.25^(priority - 1)
// and vruntime += slice * 1024 / weight, so a higher-priority process builds up
// vruntime more slowly and therefore receives a larger share of the CPU.
struct CFSCompare {
    bool operator()(const Process* a, const Process* b) {
        if (a->vruntime != b->vruntime) return a->vruntime > b->vruntime;  // Min-heap on vruntime
        return a->arrival_time > b->arrival_time;
    }
};

double weightOf(const Process& p) { return 1024.0 / std::pow(1.25, p.priority - 1); }

int main() {
    std::vector<Process> processes = defaultProcesses();  // Already in arrival order
    printHeader("Exercise 9: Completely Fair Scheduler (simplified, slice = 2)", processes);

    for (auto& p : processes) p.remaining_time = p.burst_time;
    std::priority_queue<Process*, std::vector<Process*>, CFSCompare> ready_queue;
    Gantt gantt;
    const int slice = 2;
    int current_time = 0;
    size_t next = 0, completed = 0;
    double min_vruntime = 0;

    std::cout << std::fixed << std::setprecision(2) << "Weights:";
    for (auto& p : processes) std::cout << " " << p.id << "=" << weightOf(p);
    std::cout << "\n\n";

    auto admit = [&]() {
        for (; next < processes.size() && processes[next].arrival_time <= current_time; ++next) {
            processes[next].vruntime = min_vruntime;  // Newcomers start level with the pack
            ready_queue.push(&processes[next]);
        }
    };

    while (completed < processes.size()) {
        admit();
        if (ready_queue.empty()) {
            addToGantt(gantt, "IDLE", 1);
            current_time++;
            continue;
        }

        // Select min vruntime, run slice, update vruntime += slice / weight
        Process* current = ready_queue.top();
        ready_queue.pop();
        min_vruntime = std::max(min_vruntime, current->vruntime);
        int run_time = std::min(slice, current->remaining_time);
        addToGantt(gantt, current->id, run_time);
        current->remaining_time -= run_time;
        current_time += run_time;
        current->vruntime += run_time * 1024.0 / weightOf(*current);

        admit();
        if (current->remaining_time > 0) ready_queue.push(current);
        else { finish(*current, current_time); completed++; }
    }

    std::cout << "Final vruntime:";
    for (auto& p : processes) std::cout << " " << p.id << "=" << p.vruntime;
    std::cout << "\n\n" << std::defaultfloat << std::setprecision(6);

    printResults(processes, gantt, current_time);
    return 0;
}
