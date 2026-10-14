// mlq.cpp - Exercise 6: Multilevel Queue Scheduling
// Compile: g++ mlq.cpp -o mlq
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

// Two queues: High (priority < 3) uses Round Robin with quantum 4, Low uses FCFS.
// The high queue is always preferred, and a high-priority arrival preempts a
// low-priority process that is running.
int main() {
    std::vector<Process> processes = defaultProcesses();  // Already in arrival order
    printHeader("Exercise 6: Multilevel Queue (High: RR q=4, Low: FCFS)", processes);

    for (auto& p : processes) p.remaining_time = p.burst_time;
    std::queue<Process*> high;
    std::deque<Process*> low;
    Gantt gantt;
    int current_time = 0;
    const int quantum = 4;
    size_t next = 0, completed = 0;

    // Assign to queues based on priority
    auto admit = [&]() {
        for (; next < processes.size() && processes[next].arrival_time <= current_time; ++next) {
            Process* p = &processes[next];
            bool is_high = p->priority < 3;
            std::cout << p->id << " (arrived t=" << p->arrival_time << ", priority " << p->priority << ") joins the "
                      << (is_high ? "HIGH queue (RR)" : "LOW queue (FCFS)") << "\n";
            if (is_high) high.push(p);
            else low.push_back(p);
        }
    };

    while (completed < processes.size()) {
        admit();
        if (!high.empty()) {  // Always prefer the high queue (RR)
            Process* current = high.front();
            high.pop();
            int run_time = std::min(quantum, current->remaining_time);
            addToGantt(gantt, current->id, run_time);
            current->remaining_time -= run_time;
            current_time += run_time;
            admit();
            if (current->remaining_time > 0) high.push(current);
            else { finish(*current, current_time); completed++; }
        } else if (!low.empty()) {  // Then low (FCFS), one unit at a time so a high arrival can preempt
            Process* current = low.front();
            addToGantt(gantt, current->id, 1);
            current->remaining_time--;
            current_time++;
            if (current->remaining_time == 0) {
                low.pop_front();
                finish(*current, current_time);
                completed++;
            }
        } else {
            addToGantt(gantt, "IDLE", 1);
            current_time++;
        }
    }
    std::cout << "\n";

    printResults(processes, gantt, current_time);
    return 0;
}
