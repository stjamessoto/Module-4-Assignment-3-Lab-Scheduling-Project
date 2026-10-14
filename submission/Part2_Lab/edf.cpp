// edf.cpp - Exercise 10: Earliest Deadline First
// Compile: g++ edf.cpp -o edf
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
    int deadline = 0;  // Absolute deadline (added for EDF)
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

// Preemptive EDF: the ready process with the nearest deadline always runs.
// Deadline = arrival + burst * 2.
// Enhancement - schedulability: a density test before the run and a report of
// every missed deadline after it.
struct EDFCompare {
    bool operator()(const Process* a, const Process* b) {
        if (a->deadline != b->deadline) return a->deadline > b->deadline;  // Soonest deadline first
        return a->arrival_time > b->arrival_time;
    }
};

int main() {
    std::vector<Process> processes = defaultProcesses();  // Already in arrival order
    printHeader("Exercise 10: Earliest Deadline First (preemptive)", processes);

    double density = 0;
    std::cout << "Deadlines (arrival + burst * 2):";
    for (auto& p : processes) {
        p.remaining_time = p.burst_time;
        p.deadline = p.arrival_time + p.burst_time * 2;
        density += static_cast<double>(p.burst_time) / (p.deadline - p.arrival_time);
        std::cout << " " << p.id << "=" << p.deadline;
    }
    // Enhancement: check schedulability
    std::cout << "\nSchedulability check: density = " << density
              << (density <= 1 ? " <= 1, all deadlines are guaranteed\n\n"
                               : " > 1, deadlines are NOT guaranteed (overload possible)\n\n");

    std::priority_queue<Process*, std::vector<Process*>, EDFCompare> ready_queue;
    Gantt gantt;
    int current_time = 0, missed = 0;
    size_t next = 0, completed = 0;

    // One time unit per step, so a nearer deadline that arrives preempts immediately
    while (completed < processes.size()) {
        while (next < processes.size() && processes[next].arrival_time <= current_time)
            ready_queue.push(&processes[next++]);

        if (ready_queue.empty()) {
            addToGantt(gantt, "IDLE", 1);
            current_time++;
            continue;
        }

        Process* current = ready_queue.top();
        addToGantt(gantt, current->id, 1);
        current->remaining_time--;
        current_time++;

        if (current->remaining_time == 0) {
            ready_queue.pop();
            finish(*current, current_time);
            completed++;
            bool late = current_time > current->deadline;
            if (late) missed++;
            std::cout << "t=" << std::setw(2) << std::right << current_time << ": " << current->id
                      << " finished, deadline " << std::setw(2) << current->deadline
                      << (late ? "  -> MISSED\n" : "  -> met\n");
        }
    }
    std::cout << "Deadlines missed: " << missed << " of " << processes.size() << "\n\n";

    printResults(processes, gantt, current_time);
    return 0;
}
