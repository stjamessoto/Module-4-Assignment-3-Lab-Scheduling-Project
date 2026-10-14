// srtf.cpp - Exercise 3: Shortest Remaining Time First (preemptive)
// Compile: g++ srtf.cpp -o srtf
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

int main() {
    std::vector<Process> processes = defaultProcesses();
    printHeader("Exercise 3: Shortest Remaining Time First (preemptive)", processes);

    for (auto& p : processes) p.remaining_time = p.burst_time;
    Gantt gantt;
    int current_time = 0;
    Process* current = nullptr;
    size_t completed = 0;
    int preemptions = 0;

    // Time loop until all done, one time unit per step
    while (completed < processes.size()) {
        // Among arrived processes, select the one with the shortest remaining_time
        Process* shortest = nullptr;
        for (auto& p : processes) {
            if (p.arrival_time <= current_time && p.remaining_time > 0 &&
                (!shortest || p.remaining_time < shortest->remaining_time))
                shortest = &p;
        }

        if (!shortest) {  // Nothing has arrived yet
            addToGantt(gantt, "IDLE", 1);
            current_time++;
            continue;
        }

        // If different from current while current is unfinished, that is a preemption
        if (current && current != shortest && current->remaining_time > 0) {
            std::cout << "t=" << current_time << ": " << shortest->id << " (remaining " << shortest->remaining_time
                      << ") preempts " << current->id << " (remaining " << current->remaining_time << ")\n";
            preemptions++;
        }
        current = shortest;

        // Run for 1 unit time, decrement remaining (addToGantt starts a new entry on a context switch)
        addToGantt(gantt, current->id, 1);
        current->remaining_time--;
        current_time++;

        if (current->remaining_time == 0) {
            finish(*current, current_time);
            completed++;
        }
    }

    std::cout << "Preemptions: " << preemptions << "\n\n";
    printResults(processes, gantt, current_time);
    return 0;
}
