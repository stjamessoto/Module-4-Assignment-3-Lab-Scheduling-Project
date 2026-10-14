// lottery.cpp - Exercise 8: Lottery Scheduling
// Compile: g++ lottery.cpp -o lottery
#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include <random>
#include <cstdlib>
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

// Tickets = 10 / priority, so higher-priority processes win more often.
// Enhancement - dynamic tickets: every ready process that loses a draw gains one
// ticket, and the winner drops back to its base amount, so nobody starves.
//
// Usage: ./lottery [seed]   (give a seed to repeat a run; otherwise it is random)
int main(int argc, char* argv[]) {
    std::vector<Process> processes = defaultProcesses();
    printHeader("Exercise 8: Lottery Scheduling (quantum = 2)", processes);

    std::random_device rd;
    unsigned seed = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : rd();
    std::mt19937 gen(seed);
    std::cout << "Random seed: " << seed << "\n";

    for (auto& p : processes) p.remaining_time = p.burst_time;
    const int quantum = 2;
    Gantt gantt;
    int current_time = 0;
    size_t completed = 0;

    // Assign tickets
    std::vector<int> base_tickets(processes.size()), tickets(processes.size());
    std::cout << "Base tickets:";
    for (size_t i = 0; i < processes.size(); ++i) {
        base_tickets[i] = tickets[i] = std::max(1, 10 / processes[i].priority);
        std::cout << " " << processes[i].id << "=" << tickets[i];
    }
    std::cout << "\n\nDraws:\n";

    while (completed < processes.size()) {
        // Build ticket list from the processes that are ready
        std::vector<size_t> ready;
        int total_tickets = 0;
        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].arrival_time <= current_time && processes[i].remaining_time > 0) {
                ready.push_back(i);
                total_tickets += tickets[i];
            }
        }
        if (ready.empty()) {
            addToGantt(gantt, "IDLE", 1);
            current_time++;
            continue;
        }

        // Draw random: the winner is whoever holds the drawn ticket number
        int draw = std::uniform_int_distribution<int>(0, total_tickets - 1)(gen);
        int ticket = draw;
        size_t winner = ready.back();
        for (size_t i : ready) {
            if (ticket < tickets[i]) { winner = i; break; }
            ticket -= tickets[i];
        }
        std::cout << "t=" << std::setw(2) << std::right << current_time << ": ticket " << std::setw(2) << draw
                  << " of " << std::setw(2) << total_tickets << " -> " << processes[winner].id << "  (";
        for (size_t i : ready) std::cout << " " << processes[i].id << ":" << tickets[i];
        std::cout << " )\n";

        // Enhancement: adjust tickets dynamically
        for (size_t i : ready) tickets[i] = (i == winner) ? base_tickets[i] : tickets[i] + 1;

        // Run for quantum
        Process& p = processes[winner];
        int run_time = std::min(quantum, p.remaining_time);
        addToGantt(gantt, p.id, run_time);
        p.remaining_time -= run_time;
        current_time += run_time;
        if (p.remaining_time == 0) {
            finish(p, current_time);
            completed++;
        }
    }
    std::cout << "\n";

    printResults(processes, gantt, current_time);
    return 0;
}
