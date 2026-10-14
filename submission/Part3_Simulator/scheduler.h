// scheduler.h - abstract base class every scheduling algorithm derives from
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <algorithm>
#include <string>
#include <vector>

#include "process.h"

class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void schedule(std::vector<Process>& processes, Gantt& gantt, int& total_time) = 0;
    virtual std::string name() const = 0;

    // Notable decisions made during the run (aging, promotions, deadline misses...)
    const std::vector<std::string>& events() const { return events_; }

protected:
    std::vector<std::string> events_;

    void note(int t, const std::string& msg) {
        events_.push_back("t=" + std::to_string(t) + ": " + msg);
    }

    // Sort by arrival and reset all per-run state
    static void prepare(std::vector<Process>& ps) {
        std::stable_sort(ps.begin(), ps.end(), [](const Process& a, const Process& b) {
            return a.arrival_time < b.arrival_time;
        });
        for (auto& p : ps) {
            p.remaining_time = p.burst_time;
            p.waiting_time = p.turnaround_time = p.completion_time = p.io_time = 0;
            p.start_time = -1;
            p.vruntime = 0;
        }
    }

    // Append to the Gantt chart, merging with the previous entry if it is the same process
    static void addGantt(Gantt& g, const std::string& id, int duration) {
        if (duration <= 0) return;
        if (!g.empty() && g.back().first == id) g.back().second += duration;
        else g.emplace_back(id, duration);
    }

    static void idleUntil(int& time, int until, Gantt& g) {
        if (until > time) {
            addGantt(g, "IDLE", until - time);
            time = until;
        }
    }

    // Run p for `duration` units; records start time and, on completion, the final times
    static void run(Process& p, int& time, int duration, Gantt& g) {
        if (p.start_time < 0) p.start_time = time;
        addGantt(g, p.id, duration);
        p.remaining_time -= duration;
        time += duration;
        if (p.remaining_time == 0) {
            p.completion_time = time;
            p.turnaround_time = time - p.arrival_time;
            p.waiting_time = p.turnaround_time - p.burst_time - p.io_time;
        }
    }

    // Earliest arrival after time t among unfinished processes (-1 if none)
    static int nextArrival(const std::vector<Process>& ps, int t) {
        for (const auto& p : ps)
            if (p.remaining_time > 0 && p.arrival_time > t) return p.arrival_time;
        return -1;
    }

    // Best arrived, unfinished process according to better(a, b); ties go to the earlier arrival
    template <class Better>
    static Process* pick(std::vector<Process>& ps, int t, Better better) {
        Process* best = nullptr;
        for (auto& p : ps)
            if (p.remaining_time > 0 && p.arrival_time <= t && (!best || better(p, *best))) best = &p;
        return best;
    }
};

#endif
