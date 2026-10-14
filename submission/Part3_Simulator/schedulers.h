// schedulers.h - the 10 scheduling algorithms, each a class derived from Scheduler
#ifndef SCHEDULERS_H
#define SCHEDULERS_H

#include <cmath>
#include <cstdio>
#include <deque>
#include <queue>
#include <random>

#include "scheduler.h"

// 1. First-Come, First-Served ------------------------------------------------
class FCFSScheduler : public Scheduler {
public:
    std::string name() const override { return "First-Come, First-Served"; }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        int t = 0;
        for (auto& p : ps) {
            idleUntil(t, p.arrival_time, g);
            run(p, t, p.burst_time, g);
        }
        total_time = t;
    }
};

// 2. Shortest Job First (non-preemptive) -------------------------------------
class SJFScheduler : public Scheduler {
public:
    std::string name() const override { return "Shortest Job First (non-preemptive)"; }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        int t = 0;
        for (size_t done = 0; done < ps.size();) {
            Process* p = pick(ps, t, [](const Process& a, const Process& b) {
                return a.burst_time < b.burst_time;
            });
            if (!p) { idleUntil(t, nextArrival(ps, t), g); continue; }
            run(*p, t, p->remaining_time, g);
            ++done;
        }
        total_time = t;
    }
};

// 3. Shortest Remaining Time First (preemptive) ------------------------------
class SRTFScheduler : public Scheduler {
public:
    std::string name() const override { return "Shortest Remaining Time First (preemptive)"; }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        int t = 0;
        for (size_t done = 0; done < ps.size();) {
            Process* p = pick(ps, t, [](const Process& a, const Process& b) {
                return a.remaining_time < b.remaining_time;
            });
            if (!p) { idleUntil(t, nextArrival(ps, t), g); continue; }
            // Run until it finishes or the next arrival, which may preempt it
            int duration = p->remaining_time;
            int next = nextArrival(ps, t);
            if (next != -1) duration = std::min(duration, next - t);
            run(*p, t, duration, g);
            if (p->remaining_time == 0) ++done;
        }
        total_time = t;
    }
};

// 4. Priority (non-preemptive) with aging ------------------------------------
// Aging: every `aging` time units a process waits, its priority improves by one
// level, so low-priority tasks cannot starve. aging = 0 disables it.
class PriorityScheduler : public Scheduler {
    int aging_;
    int effective(const Process& p, int t) const {
        if (aging_ <= 0) return p.priority;
        return std::max(1, p.priority - (t - p.arrival_time) / aging_);
    }
public:
    explicit PriorityScheduler(int aging) : aging_(aging) {}
    std::string name() const override {
        return aging_ > 0 ? "Priority (non-preemptive, aging every " + std::to_string(aging_) + " units)"
                          : "Priority (non-preemptive, no aging)";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        int t = 0;
        for (size_t done = 0; done < ps.size();) {
            Process* p = pick(ps, t, [&](const Process& a, const Process& b) {
                return effective(a, t) < effective(b, t);
            });
            if (!p) { idleUntil(t, nextArrival(ps, t), g); continue; }
            Process* plain = pick(ps, t, [](const Process& a, const Process& b) {
                return a.priority < b.priority;
            });
            if (plain != p)
                note(t, "aging: " + p->id + " (priority " + std::to_string(p->priority) + " aged to " +
                            std::to_string(effective(*p, t)) + ") runs ahead of " + plain->id +
                            " (priority " + std::to_string(plain->priority) + ")");
            run(*p, t, p->remaining_time, g);
            ++done;
        }
        total_time = t;
    }
};

// 5. Round Robin, with optional I/O simulation -------------------------------
// I/O: after a time slice an unfinished process blocks for 1-5 units with
// probability io_percent, then rejoins the back of the ready queue.
class RoundRobinScheduler : public Scheduler {
    int quantum_;
    int io_percent_;
    std::mt19937 rng_;
public:
    explicit RoundRobinScheduler(int q, int io_percent = 0, unsigned seed = 0)
        : quantum_(q), io_percent_(io_percent), rng_(seed) {}
    std::string name() const override {
        std::string s = "Round Robin (quantum = " + std::to_string(quantum_) + ")";
        if (io_percent_ > 0) s += " with I/O simulation (" + std::to_string(io_percent_) + "% block chance)";
        return s;
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        std::queue<Process*> ready;
        std::vector<std::pair<int, Process*>> blocked;  // (wake-up time, process)
        size_t idx = 0, done = 0;
        int t = 0;

        auto admit = [&]() {
            while (idx < ps.size() && ps[idx].arrival_time <= t) ready.push(&ps[idx++]);
            std::stable_sort(blocked.begin(), blocked.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            size_t woke = 0;
            while (woke < blocked.size() && blocked[woke].first <= t) ready.push(blocked[woke++].second);
            blocked.erase(blocked.begin(), blocked.begin() + woke);
        };

        while (done < ps.size()) {
            admit();
            if (ready.empty()) {  // Idle until the next arrival or I/O completion
                int next = (idx < ps.size()) ? ps[idx].arrival_time : blocked.front().first;
                for (const auto& b : blocked) next = std::min(next, b.first);
                idleUntil(t, next, g);
                continue;
            }
            Process* cur = ready.front();
            ready.pop();
            run(*cur, t, std::min(quantum_, cur->remaining_time), g);
            admit();  // Arrivals during the slice queue ahead of the preempted process

            if (cur->remaining_time == 0) {
                ++done;
            } else if (io_percent_ > 0 && static_cast<int>(rng_() % 100) < io_percent_) {
                int io = 1 + rng_() % 5;
                cur->io_time += io;
                blocked.emplace_back(t + io, cur);
                note(t, cur->id + " blocks for I/O (" + std::to_string(io) + " units)");
            } else {
                ready.push(cur);
            }
        }
        total_time = t;
    }
};

// 6. Multilevel Queue --------------------------------------------------------
// High queue (priority < 3): Round Robin. Low queue: FCFS.
// The high queue always wins; a high arrival preempts a running low process.
class MLQScheduler : public Scheduler {
    int quantum_;
    static constexpr int kHighBelow = 3;
public:
    explicit MLQScheduler(int q) : quantum_(q) {}
    std::string name() const override {
        return "Multilevel Queue (high: RR q=" + std::to_string(quantum_) + " for priority < 3, low: FCFS)";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        std::queue<Process*> high;
        std::deque<Process*> low;
        size_t idx = 0, done = 0;
        int t = 0;

        auto admit = [&]() {
            for (; idx < ps.size() && ps[idx].arrival_time <= t; ++idx) {
                if (ps[idx].priority < kHighBelow) high.push(&ps[idx]);
                else low.push_back(&ps[idx]);
            }
        };

        while (done < ps.size()) {
            admit();
            if (!high.empty()) {
                Process* cur = high.front();
                high.pop();
                run(*cur, t, std::min(quantum_, cur->remaining_time), g);
                admit();
                if (cur->remaining_time > 0) high.push(cur);
                else ++done;
            } else if (!low.empty()) {
                // FCFS, but only until the next arrival in case it belongs to the high queue
                Process* cur = low.front();
                int duration = cur->remaining_time;
                if (idx < ps.size()) duration = std::min(duration, ps[idx].arrival_time - t);
                run(*cur, t, duration, g);
                if (cur->remaining_time == 0) { low.pop_front(); ++done; }
            } else {
                idleUntil(t, ps[idx].arrival_time, g);
            }
        }
        total_time = t;
    }
};

// 7. Multilevel Feedback Queue -----------------------------------------------
// Three RR queues (quanta 2, 4, 8). Everyone starts at the top; using a full
// quantum demotes a process one level (CPU-bound work sinks). Feedback in the
// other direction: waiting `promote_after_` units in a lower queue promotes it.
class MLFQScheduler : public Scheduler {
    std::vector<int> quanta_{2, 4, 8};
    int promote_after_;
public:
    explicit MLFQScheduler(int promote_after = 8) : promote_after_(promote_after) {}
    std::string name() const override {
        return "Multilevel Feedback Queue (quanta 2/4/8, promote after waiting " +
               std::to_string(promote_after_) + ")";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        const int levels = static_cast<int>(quanta_.size());
        std::vector<std::deque<size_t>> queues(levels);
        std::vector<int> queued_at(ps.size(), 0);
        size_t idx = 0, done = 0;
        int t = 0;

        auto enqueue = [&](size_t i, int level) {
            queues[level].push_back(i);
            queued_at[i] = t;
        };
        auto admit = [&]() {
            while (idx < ps.size() && ps[idx].arrival_time <= t) enqueue(idx++, 0);
        };

        while (done < ps.size()) {
            admit();
            // Promote anything that has waited too long in a lower queue
            for (int l = 1; l < levels; ++l) {
                for (auto it = queues[l].begin(); it != queues[l].end();) {
                    size_t i = *it;
                    if (t - queued_at[i] >= promote_after_) {
                        it = queues[l].erase(it);
                        enqueue(i, l - 1);
                        note(t, ps[i].id + " promoted Q" + std::to_string(l) + " -> Q" + std::to_string(l - 1) +
                                    " (waited too long)");
                    } else {
                        ++it;
                    }
                }
            }

            int level = 0;
            while (level < levels && queues[level].empty()) ++level;
            if (level == levels) { idleUntil(t, ps[idx].arrival_time, g); continue; }

            size_t i = queues[level].front();
            queues[level].pop_front();
            Process& p = ps[i];
            int used = 0;
            while (used < quanta_[level] && p.remaining_time > 0) {
                run(p, t, 1, g);
                ++used;
                admit();
                if (level > 0 && !queues[0].empty()) break;  // A new arrival in Q0 preempts
            }

            if (p.remaining_time == 0) {
                ++done;
            } else if (used == quanta_[level]) {
                int next = std::min(level + 1, levels - 1);
                if (next != level)
                    note(t, p.id + " used its full quantum, demoted Q" + std::to_string(level) + " -> Q" +
                                std::to_string(next));
                enqueue(i, next);
            } else {
                enqueue(i, level);  // Preempted early: stays at its level
            }
        }
        total_time = t;
    }
};

// 8. Lottery -----------------------------------------------------------------
// Base tickets = 10 / priority. Dynamic adjustment: every ready process that
// loses a draw gains one ticket; the winner drops back to its base amount.
class LotteryScheduler : public Scheduler {
    int quantum_;
    std::mt19937 rng_;
public:
    LotteryScheduler(int q, unsigned seed) : quantum_(q), rng_(seed) {}
    std::string name() const override {
        return "Lottery (tickets = 10 / priority, quantum = " + std::to_string(quantum_) + ")";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        auto base = [](const Process& p) { return std::max(1, 10 / std::max(1, p.priority)); };
        std::vector<int> tickets;
        for (const auto& p : ps) tickets.push_back(base(p));
        int t = 0;

        for (size_t done = 0; done < ps.size();) {
            std::vector<size_t> ready;
            int pool = 0;
            for (size_t i = 0; i < ps.size(); ++i)
                if (ps[i].remaining_time > 0 && ps[i].arrival_time <= t) { ready.push_back(i); pool += tickets[i]; }
            if (ready.empty()) { idleUntil(t, nextArrival(ps, t), g); continue; }

            int draw = std::uniform_int_distribution<int>(0, pool - 1)(rng_);
            size_t winner = ready.back();
            for (size_t i : ready) {
                if (draw < tickets[i]) { winner = i; break; }
                draw -= tickets[i];
            }
            for (size_t i : ready) tickets[i] = (i == winner) ? base(ps[i]) : tickets[i] + 1;

            run(ps[winner], t, std::min(quantum_, ps[winner].remaining_time), g);
            if (ps[winner].remaining_time == 0) ++done;
        }
        total_time = t;
    }
};

// 9. Completely Fair Scheduler (simplified) ----------------------------------
// Always runs the process with the smallest virtual runtime (min-heap).
// vruntime += slice * 1024 / weight, where weight shrinks by 1.25x per priority
// level, so higher-priority tasks accumulate vruntime slower and get more CPU.
class CFSScheduler : public Scheduler {
    int slice_;
    struct Compare {
        bool operator()(const Process* a, const Process* b) const {
            if (a->vruntime != b->vruntime) return a->vruntime > b->vruntime;  // Min-heap on vruntime
            return a->arrival_time > b->arrival_time;
        }
    };
    static double weight(int priority) { return 1024.0 / std::pow(1.25, std::max(1, priority) - 1); }
public:
    explicit CFSScheduler(int slice = 2) : slice_(slice) {}
    std::string name() const override {
        return "Completely Fair Scheduler (simplified, slice = " + std::to_string(slice_) + ")";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        std::priority_queue<Process*, std::vector<Process*>, Compare> ready;
        size_t idx = 0, done = 0;
        int t = 0;
        double min_vruntime = 0;

        auto admit = [&]() {
            for (; idx < ps.size() && ps[idx].arrival_time <= t; ++idx) {
                ps[idx].vruntime = min_vruntime;  // Newcomers start level with the pack
                ready.push(&ps[idx]);
            }
        };

        while (done < ps.size()) {
            admit();
            if (ready.empty()) { idleUntil(t, ps[idx].arrival_time, g); continue; }
            Process* cur = ready.top();
            ready.pop();
            min_vruntime = std::max(min_vruntime, cur->vruntime);
            int duration = std::min(slice_, cur->remaining_time);
            run(*cur, t, duration, g);
            cur->vruntime += duration * 1024.0 / weight(cur->priority);
            admit();
            if (cur->remaining_time > 0) ready.push(cur);
            else ++done;
        }
        total_time = t;
    }
};

// 10. Earliest Deadline First (preemptive) -----------------------------------
// Tasks without a deadline in the input get arrival + burst * factor.
// Reports a schedulability estimate up front and every missed deadline.
class EDFScheduler : public Scheduler {
    int factor_;
    struct Compare {
        bool operator()(const Process* a, const Process* b) const {
            if (a->deadline != b->deadline) return a->deadline > b->deadline;  // Soonest deadline first
            return a->arrival_time > b->arrival_time;
        }
    };
public:
    explicit EDFScheduler(int factor = 2) : factor_(factor) {}
    std::string name() const override {
        return "Earliest Deadline First (preemptive, default deadline = arrival + burst x " +
               std::to_string(factor_) + ")";
    }
    void schedule(std::vector<Process>& ps, Gantt& g, int& total_time) override {
        prepare(ps);
        double density = 0;
        bool impossible = false;
        for (auto& p : ps) {
            if (p.deadline <= 0) p.deadline = p.arrival_time + p.burst_time * factor_;
            int window = p.deadline - p.arrival_time;
            if (window < p.burst_time) impossible = true;
            else density += static_cast<double>(p.burst_time) / window;
        }
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", density);
        if (impossible) note(0, "schedulability: at least one deadline is shorter than its burst - misses are certain");
        else if (density <= 1.0) note(0, std::string("schedulability: density ") + buf + " <= 1, all deadlines are guaranteed");
        else note(0, std::string("schedulability: density ") + buf + " > 1, deadlines are not guaranteed (overload possible)");

        std::priority_queue<Process*, std::vector<Process*>, Compare> ready;
        size_t idx = 0, done = 0;
        int t = 0, missed = 0;
        while (done < ps.size()) {
            while (idx < ps.size() && ps[idx].arrival_time <= t) ready.push(&ps[idx++]);
            if (ready.empty()) { idleUntil(t, ps[idx].arrival_time, g); continue; }
            // Run the nearest deadline until it finishes or the next arrival (possible preemption)
            Process* cur = ready.top();
            int duration = cur->remaining_time;
            if (idx < ps.size()) duration = std::min(duration, ps[idx].arrival_time - t);
            run(*cur, t, duration, g);
            if (cur->remaining_time == 0) {
                ready.pop();
                ++done;
                if (t > cur->deadline) {
                    ++missed;
                    note(t, cur->id + " MISSED its deadline " + std::to_string(cur->deadline) + " (finished at " +
                                std::to_string(t) + ")");
                }
            }
        }
        if (missed == 0) note(t, "all deadlines met");
        else note(t, std::to_string(missed) + " of " + std::to_string(ps.size()) +
                         " deadlines missed (EDF is optimal on one CPU, so no order could meet them all)");
        total_time = t;
    }
};

#endif
