// simulator.cpp - modular command-line task scheduling simulator
//
// Build:  g++ -std=c++17 -pthread simulator.cpp -o simulator
// Help:   ./simulator --help
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "process.h"
#include "scheduler.h"
#include "schedulers.h"
#include "utils.h"

const std::vector<std::string> kSchedulers = {"fcfs", "sjf", "srtf", "priority", "rr",
                                              "mlq", "mlfq", "lottery", "cfs", "edf"};

struct Options {
    std::string scheduler, input, output;
    std::vector<std::string> compare;  // Non-empty = comparison mode
    bool random = false, web = false, verbose = false;
    int quantum = 4, num = 10, aging = 5, io = 0, cpus = 1;
    unsigned seed = 0;
};

void printUsage() {
    std::cout <<
        "Task Scheduling Simulator\n"
        "\n"
        "Usage:\n"
        "  ./simulator --scheduler <name> [options]     run one scheduler\n"
        "  ./simulator --compare [a,b,...] [options]    compare schedulers on the same tasks\n"
        "\n"
        "Schedulers:\n"
        "  fcfs      First-Come, First-Served\n"
        "  sjf       Shortest Job First (non-preemptive)\n"
        "  srtf      Shortest Remaining Time First (preemptive)\n"
        "  priority  Priority (non-preemptive) with aging\n"
        "  rr        Round Robin\n"
        "  mlq       Multilevel Queue (high: RR, low: FCFS)\n"
        "  mlfq      Multilevel Feedback Queue (3 levels)\n"
        "  lottery   Lottery scheduling\n"
        "  cfs       Completely Fair Scheduler (simplified)\n"
        "  edf       Earliest Deadline First\n"
        "\n"
        "Tasks (default: the 4-process table from the assignment):\n"
        "  --input <file>    read tasks, one per line: ID arrival burst priority [deadline]\n"
        "  --random          generate random tasks\n"
        "  --web             generate a web-server workload (requests, mostly short)\n"
        "  --num <n>         how many tasks --random / --web generate (default 10)\n"
        "  --seed <n>        seed for anything random, to repeat a run exactly\n"
        "\n"
        "Scheduler settings:\n"
        "  --quantum <n>     time quantum for rr, mlq and lottery (default 4)\n"
        "  --aging <n>       priority: improve priority every n units waited (default 5, 0 = off)\n"
        "  --io <percent>    rr: chance a process blocks for I/O after a time slice (default 0)\n"
        "  --cpus <n>        number of CPUs; tasks are load-balanced across them (default 1)\n"
        "\n"
        "Output:\n"
        "  --output <file>   also write the results to a file\n"
        "  --verbose         show every scheduler event instead of the first 15\n"
        "  --help            show this message\n"
        "\n"
        "Examples:\n"
        "  ./simulator --scheduler rr --quantum 4\n"
        "  ./simulator --scheduler srtf --input tasks.txt --output log.txt\n"
        "  ./simulator --compare --random --num 20 --seed 42\n"
        "  ./simulator --compare rr,srtf --input tasks.txt\n";
}

bool parseInt(const std::string& option, const std::string& text, int min, int max, int& out) {
    try {
        size_t used = 0;
        int value = std::stoi(text, &used);
        if (used == text.size() && value >= min && value <= max) {
            out = value;
            return true;
        }
    } catch (const std::exception&) {
    }
    std::cerr << "Error: " << option << " needs a whole number from " << min << " to " << max << " (got '" << text
              << "')\n";
    return false;
}

bool isScheduler(const std::string& name) {
    return std::find(kSchedulers.begin(), kSchedulers.end(), name) != kSchedulers.end();
}

void reportUnknownScheduler(const std::string& name) {
    std::cerr << "Error: unknown scheduler '" << name << "'. Choose one of:";
    for (const auto& s : kSchedulers) std::cerr << " " << s;
    std::cerr << "\n";
}

// Returns false (after printing why) if the command line is invalid
bool parseArgs(int argc, char* argv[], Options& opt, bool& help) {
    const std::set<std::string> needs_value = {"--scheduler", "--input", "--output", "--num",  "--seed",
                                               "--quantum",   "--aging", "--io",     "--cpus"};
    const std::set<std::string> flags = {"--random", "--web", "--verbose", "--help", "--compare"};

    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "-h") key = "--help";
        if (!needs_value.count(key) && !flags.count(key)) {
            std::cerr << "Error: unknown option '" << key << "' (try --help)\n";
            return false;
        }
        bool has_value = i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0;
        if (needs_value.count(key) && !has_value) {
            std::cerr << "Error: " << key << " needs a value (try --help)\n";
            return false;
        }
        args[key] = has_value ? argv[++i] : "";
    }

    help = args.count("--help");
    opt.scheduler = args["--scheduler"];
    opt.input = args["--input"];
    opt.output = args["--output"];
    opt.random = args.count("--random");
    opt.web = args.count("--web");
    opt.verbose = args.count("--verbose");
    opt.seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());

    int seed = 0;
    if (args.count("--seed")) {
        if (!parseInt("--seed", args["--seed"], 0, 2000000000, seed)) return false;
        opt.seed = seed;
    }
    // "--random 20" is accepted as shorthand for "--random --num 20"
    for (const char* key : {"--random", "--web", "--num"})
        if (!args[key].empty() && !parseInt(key, args[key], 1, 100000, opt.num)) return false;
    if (args.count("--quantum") && !parseInt("--quantum", args["--quantum"], 1, 1000, opt.quantum)) return false;
    if (args.count("--aging") && !parseInt("--aging", args["--aging"], 0, 1000000, opt.aging)) return false;
    if (args.count("--io") && !parseInt("--io", args["--io"], 0, 90, opt.io)) return false;
    if (args.count("--cpus") && !parseInt("--cpus", args["--cpus"], 1, 64, opt.cpus)) return false;

    if (args.count("--compare")) {
        std::istringstream list(args["--compare"]);
        for (std::string name; std::getline(list, name, ',');) {
            if (!isScheduler(name)) { reportUnknownScheduler(name); return false; }
            opt.compare.push_back(name);
        }
        if (opt.compare.empty()) opt.compare = kSchedulers;
    }
    return true;
}

std::unique_ptr<Scheduler> makeScheduler(const std::string& type, const Options& opt, unsigned seed) {
    if (type == "fcfs") return std::make_unique<FCFSScheduler>();
    if (type == "sjf") return std::make_unique<SJFScheduler>();
    if (type == "srtf") return std::make_unique<SRTFScheduler>();
    if (type == "priority") return std::make_unique<PriorityScheduler>(opt.aging);
    if (type == "rr") return std::make_unique<RoundRobinScheduler>(opt.quantum, opt.io, seed);
    if (type == "mlq") return std::make_unique<MLQScheduler>(opt.quantum);
    if (type == "mlfq") return std::make_unique<MLFQScheduler>();
    if (type == "lottery") return std::make_unique<LotteryScheduler>(opt.quantum, seed);
    if (type == "cfs") return std::make_unique<CFSScheduler>();
    if (type == "edf") return std::make_unique<EDFScheduler>();
    return nullptr;
}

struct RunResult {
    std::string name;
    std::vector<Process> processes;
    std::vector<Gantt> gantts;  // One per CPU
    std::vector<std::string> events;
    int total_time = 0;
};

// Runs one scheduler type over the tasks. With several CPUs, a load balancer
// sends each task to the CPU that will be free soonest, then every CPU runs
// its own scheduler instance in its own thread.
RunResult simulate(const std::string& type, const Options& opt, const std::vector<Process>& processes) {
    const int cpus = opt.cpus;
    std::vector<std::vector<Process>> share(cpus);
    std::vector<int> free_at(cpus, 0);
    for (const auto& p : processes) {
        int target = std::min_element(free_at.begin(), free_at.end()) - free_at.begin();
        free_at[target] = std::max(free_at[target], p.arrival_time) + p.burst_time;
        share[target].push_back(p);
    }

    std::vector<std::unique_ptr<Scheduler>> schedulers;
    for (int c = 0; c < cpus; ++c) schedulers.push_back(makeScheduler(type, opt, opt.seed + c));

    RunResult result;
    result.name = schedulers[0]->name();
    result.gantts.resize(cpus);
    std::vector<int> totals(cpus, 0);
    if (cpus == 1) {
        schedulers[0]->schedule(share[0], result.gantts[0], totals[0]);
    } else {
        std::vector<std::thread> workers;
        for (int c = 0; c < cpus; ++c)
            workers.emplace_back([&, c] { schedulers[c]->schedule(share[c], result.gantts[c], totals[c]); });
        for (auto& w : workers) w.join();
    }

    for (int c = 0; c < cpus; ++c) {
        result.total_time = std::max(result.total_time, totals[c]);
        for (auto& p : share[c]) {
            if (cpus > 1) p.cpu = c + 1;
            result.processes.push_back(p);
        }
        for (const auto& e : schedulers[c]->events())
            result.events.push_back(cpus > 1 ? "[CPU " + std::to_string(c + 1) + "] " + e : e);
    }
    sortByArrival(result.processes);
    return result;
}

void printHeader(std::ostream& out, const std::string& workload, const std::vector<Process>& processes,
                 const Options& opt) {
    out << "==============================================================\n"
        << " Task Scheduling Simulator\n"
        << "==============================================================\n"
        << "Workload : " << workload << " (" << processes.size() << " tasks)\n"
        << "CPUs     : " << opt.cpus << "\n";
}

void reportSingle(std::ostream& out, const RunResult& r, const Options& opt) {
    out << "Scheduler: " << r.name << "\n\n";
    for (size_t c = 0; c < r.gantts.size(); ++c) {
        if (r.gantts.size() > 1) out << "--- CPU " << c + 1 << " ---\n";
        printGantt(out, r.gantts[c]);
    }
    out << "\n";
    printProcessTable(out, r.processes, true);
    out << "\n";
    printMetrics(out, calculateMetrics(r.processes, r.gantts, r.total_time));

    if (!r.events.empty()) {
        out << "\nScheduler events:\n";
        size_t shown = opt.verbose ? r.events.size() : std::min<size_t>(r.events.size(), 15);
        for (size_t i = 0; i < shown; ++i) out << "  " << r.events[i] << "\n";
        if (shown < r.events.size())
            out << "  ... " << r.events.size() - shown << " more (use --verbose to see all)\n";
    }
}

void reportComparison(std::ostream& out, const std::vector<Process>& processes, const Options& opt) {
    bool deadlines = false;
    for (const auto& p : processes) deadlines |= p.deadline > 0;

    out << "\nInput tasks:\n";
    printProcessTable(out, processes, false);
    out << "\nComparison (same tasks for every scheduler, quantum = " << opt.quantum << "):\n";
    out << std::left << std::setw(10) << "Scheduler" << std::right << std::setw(10) << "Avg Wait" << std::setw(16)
        << "Avg Turnaround" << std::setw(14) << "Avg Response" << std::setw(11) << "CPU Util" << std::setw(12)
        << "Throughput" << std::setw(10) << "Switches";
    if (deadlines) out << std::setw(8) << "Missed";
    out << "\n" << std::string(deadlines ? 91 : 83, '-') << "\n";

    std::string best_wait, best_response;
    double min_wait = 0, min_response = 0;
    for (const auto& type : opt.compare) {
        RunResult r = simulate(type, opt, processes);
        Metrics m = calculateMetrics(r.processes, r.gantts, r.total_time);
        out << std::left << std::setw(10) << type << std::right << std::fixed << std::setprecision(2)
            << std::setw(10) << m.avg_wait << std::setw(16) << m.avg_turn << std::setw(14) << m.avg_response
            << std::setw(10) << m.cpu_util << "%" << std::setw(12) << std::setprecision(4) << m.throughput
            << std::setw(10) << m.switches;
        if (deadlines) out << std::setw(8) << m.missed;
        out << "\n";
        if (best_wait.empty() || m.avg_wait < min_wait) { best_wait = type; min_wait = m.avg_wait; }
        if (best_response.empty() || m.avg_response < min_response) { best_response = type; min_response = m.avg_response; }
    }
    out << std::setprecision(2) << "\nLowest average waiting time : " << best_wait << " (" << min_wait << ")\n"
        << "Lowest average response time: " << best_response << " (" << min_response << ")\n";
}

int main(int argc, char* argv[]) {
    Options opt;
    bool help = false;
    if (!parseArgs(argc, argv, opt, help)) return 1;
    if (help || argc == 1) {
        printUsage();
        return 0;
    }

    if (opt.compare.empty()) {
        if (opt.scheduler.empty()) {
            std::cerr << "Error: choose a scheduler with --scheduler <name>, or use --compare (try --help)\n";
            return 1;
        }
        if (!isScheduler(opt.scheduler)) {
            reportUnknownScheduler(opt.scheduler);
            return 1;
        }
        if (opt.io > 0 && opt.scheduler != "rr")
            std::cerr << "Warning: --io is only simulated by the rr scheduler; ignoring it\n";
    }

    std::vector<Process> processes;
    std::string workload;
    if (opt.random) {
        processes = generateRandomProcesses(opt.num, opt.seed);
        workload = "random tasks, seed " + std::to_string(opt.seed);
    } else if (opt.web) {
        processes = generateWebRequests(opt.num, opt.seed);
        workload = "web-server requests, seed " + std::to_string(opt.seed);
    } else if (!opt.input.empty()) {
        if (!loadProcesses(opt.input, processes)) return 1;
        workload = "file " + opt.input;
    } else {
        processes = defaultProcesses();
        workload = "default assignment table";
    }
    if (processes.empty()) {
        std::cerr << "No processes loaded.\n";
        return 1;
    }

    std::ostringstream report;
    printHeader(report, workload, processes, opt);
    if (opt.compare.empty()) reportSingle(report, simulate(opt.scheduler, opt, processes), opt);
    else reportComparison(report, processes, opt);
    std::cout << report.str();

    if (!opt.output.empty()) {
        std::ofstream log(opt.output);
        if (!log) {
            std::cerr << "Error: cannot write output file '" << opt.output << "'\n";
            return 1;
        }
        log << report.str();
        std::cout << "\nResults written to " << opt.output << "\n";
    }
    return 0;
}
