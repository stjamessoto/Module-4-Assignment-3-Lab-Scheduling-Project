// utils.h - workload loading/generation, metrics and report printing
#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "process.h"

inline void sortByArrival(std::vector<Process>& processes) {
    std::stable_sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
        return a.arrival_time < b.arrival_time;
    });
}

// The 4-process table from the assignment
inline std::vector<Process> defaultProcesses() {
    return {
        {"P1", 0, 8, 2, 8},
        {"P2", 1, 4, 1, 4},
        {"P3", 2, 9, 3, 9},
        {"P4", 3, 5, 4, 5}
    };
}

// File format, one task per line:  ID arrival burst priority [deadline]
// '#' starts a comment. Bad lines are reported and skipped. Returns false if the file cannot be opened.
inline bool loadProcesses(const std::string& filename, std::vector<Process>& processes) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: cannot open input file '" << filename << "'\n";
        return false;
    }
    std::string line;
    for (int line_no = 1; std::getline(file, line); ++line_no) {
        line = line.substr(0, line.find('#'));
        std::istringstream in(line);
        std::string id;
        if (!(in >> id)) continue;  // Blank or comment-only line
        int at, bt, pri, deadline = 0;
        if (!(in >> at >> bt >> pri)) {
            std::cerr << "Warning: " << filename << " line " << line_no
                      << " skipped (expected: ID arrival burst priority [deadline])\n";
            continue;
        }
        if (!(in >> deadline)) deadline = 0;
        if (at < 0 || bt <= 0 || pri < 1 || deadline < 0) {
            std::cerr << "Warning: " << filename << " line " << line_no
                      << " skipped (need arrival >= 0, burst > 0, priority >= 1, deadline >= 0)\n";
            continue;
        }
        Process p{id, at, bt, pri, bt};
        p.deadline = deadline;
        processes.push_back(p);
    }
    sortByArrival(processes);
    return true;
}

inline std::vector<Process> generateRandomProcesses(int num, unsigned seed) {
    std::vector<Process> processes;
    std::mt19937 gen(seed);
    for (int i = 0; i < num; ++i) {
        std::string id = "P" + std::to_string(i + 1);
        int at = gen() % 20;
        int bt = 1 + gen() % 10;
        int pri = 1 + gen() % 5;
        processes.push_back({id, at, bt, pri, bt});
    }
    sortByArrival(processes);
    return processes;
}

// Web-server workload: a steady stream of requests, mostly short (page/API calls)
// with an occasional long one (report, upload). Burst = processing time.
inline std::vector<Process> generateWebRequests(int num, unsigned seed) {
    std::vector<Process> requests;
    std::mt19937 gen(seed);
    int at = 0;
    for (int i = 0; i < num; ++i) {
        at += gen() % 3;
        bool heavy = gen() % 5 == 0;
        int bt = heavy ? 8 + gen() % 8 : 1 + gen() % 3;
        int pri = heavy ? 3 + gen() % 3 : 1 + gen() % 3;
        requests.push_back({"REQ" + std::to_string(i + 1), at, bt, pri, bt});
    }
    return requests;
}

struct Metrics {
    double avg_wait = 0, avg_turn = 0, avg_response = 0, cpu_util = 0, throughput = 0;
    int switches = 0;   // Times the CPU changed from one process to another
    int deadlines = 0;  // Processes that have a deadline
    int missed = 0;     // ...and finished after it
};

inline Metrics calculateMetrics(const std::vector<Process>& processes, const std::vector<Gantt>& gantts,
                                int total_time) {
    Metrics m;
    int busy = 0;
    for (const auto& p : processes) {
        m.avg_wait += p.waiting_time;
        m.avg_turn += p.turnaround_time;
        m.avg_response += p.start_time - p.arrival_time;
        busy += p.burst_time;
        if (p.deadline > 0) {
            ++m.deadlines;
            if (p.completion_time > p.deadline) ++m.missed;
        }
    }
    int n = processes.size();
    m.avg_wait /= n;
    m.avg_turn /= n;
    m.avg_response /= n;
    if (total_time > 0) {
        m.cpu_util = static_cast<double>(busy) / (static_cast<double>(total_time) * gantts.size()) * 100;
        m.throughput = static_cast<double>(n) / total_time;
    }
    for (const auto& gantt : gantts) {
        int runs = 0;
        for (const auto& entry : gantt)
            if (entry.first != "IDLE") ++runs;
        m.switches += std::max(0, runs - 1);
    }
    return m;
}

inline void printGantt(std::ostream& out, const Gantt& gantt) {
    out << "Gantt Chart: ";
    for (const auto& entry : gantt) out << entry.first << "(" << entry.second << ") ";
    out << "\nTimeline:    0";
    int t = 0;
    for (const auto& entry : gantt) {
        t += entry.second;
        out << " [" << entry.first << "] " << t;
    }
    out << "\n";
}

inline void printProcessTable(std::ostream& out, const std::vector<Process>& processes, bool finished) {
    bool deadlines = false, io = false, cpus = false;
    for (const auto& p : processes) {
        deadlines |= p.deadline > 0;
        io |= p.io_time > 0;
        cpus |= p.cpu > 0;
    }
    out << std::left << std::setw(8) << "ID" << std::right << std::setw(8) << "Arrival" << std::setw(7) << "Burst"
        << std::setw(10) << "Priority";
    if (deadlines) out << std::setw(10) << "Deadline";
    if (finished) {
        out << std::setw(7) << "Start" << std::setw(8) << "Finish" << std::setw(9) << "Waiting"
            << std::setw(12) << "Turnaround" << std::setw(10) << "Response";
        if (io) out << std::setw(5) << "I/O";
        if (cpus) out << std::setw(5) << "CPU";
    }
    out << "\n";
    for (const auto& p : processes) {
        out << std::left << std::setw(8) << p.id << std::right << std::setw(8) << p.arrival_time << std::setw(7)
            << p.burst_time << std::setw(10) << p.priority;
        if (deadlines) out << std::setw(10) << (p.deadline > 0 ? std::to_string(p.deadline) : "-");
        if (finished) {
            out << std::setw(7) << p.start_time << std::setw(8) << p.completion_time << std::setw(9)
                << p.waiting_time << std::setw(12) << p.turnaround_time << std::setw(10)
                << p.start_time - p.arrival_time;
            if (io) out << std::setw(5) << p.io_time;
            if (cpus) out << std::setw(5) << p.cpu;
        }
        out << "\n";
    }
}

inline void printMetrics(std::ostream& out, const Metrics& m) {
    out << std::fixed << std::setprecision(2);
    out << "Average Waiting Time: " << m.avg_wait << "\n";
    out << "Average Turnaround Time: " << m.avg_turn << "\n";
    out << "Average Response Time: " << m.avg_response << "\n";
    out << "CPU Utilization: " << m.cpu_util << "%\n";
    out << std::setprecision(4) << "Throughput: " << m.throughput << " processes/unit time\n";
    out << "Context Switches: " << m.switches << "\n";
    if (m.deadlines > 0) out << "Deadlines Missed: " << m.missed << " of " << m.deadlines << "\n";
}

#endif
