// priority.cpp - Exercise 4: Priority Scheduling (non-preemptive) with aging
// Compile: g++ priority.cpp -o priority
#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
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

// Enhancement - aging: for every AGING_INTERVAL time units a process has waited,
// its priority improves by one level, so low-priority processes cannot starve.
const int AGING_INTERVAL = 5;

bool comparePriority(const Process& a, const Process& b) {
    return a.priority < b.priority;  // Lower number higher priority
}

int main() {
    std::vector<Process> processes = defaultProcesses();  // Already in arrival order
    printHeader("Exercise 4: Priority Scheduling (non-preemptive) with aging", processes);

    Gantt gantt;
    int current_time = 0;
    std::vector<Process*> ready_queue;
    size_t next = 0;

    // A copy of the process whose priority reflects how long it has waited
    auto aged = [&](const Process& p) {
        Process copy = p;
        copy.priority = std::max(1, p.priority - (current_time - p.arrival_time) / AGING_INTERVAL);
        return copy;
    };

    std::cout << "Scheduling log (aging: +1 priority level per " << AGING_INTERVAL << " units waited):\n";
    while (next < processes.size() || !ready_queue.empty()) {
        while (next < processes.size() && processes[next].arrival_time <= current_time)
            ready_queue.push_back(&processes[next++]);

        if (ready_queue.empty()) {
            addToGantt(gantt, "IDLE", processes[next].arrival_time - current_time);
            current_time = processes[next].arrival_time;
            continue;
        }

        // Similar to SJF but sort by (aged) priority
        std::stable_sort(ready_queue.begin(), ready_queue.end(), [&](const Process* a, const Process* b) {
            return comparePriority(aged(*a), aged(*b));
        });

        Process* p = ready_queue.front();
        ready_queue.erase(ready_queue.begin());
        std::cout << "t=" << std::setw(2) << std::right << current_time << ": run " << p->id << "  (priority "
                  << p->priority << ", aged to " << aged(*p).priority << " after waiting "
                  << current_time - p->arrival_time << ")\n";

        addToGantt(gantt, p->id, p->burst_time);
        current_time += p->burst_time;
        p->remaining_time = 0;
        finish(*p, current_time);
    }
    std::cout << "\n";

    printResults(processes, gantt, current_time);
    return 0;
}
