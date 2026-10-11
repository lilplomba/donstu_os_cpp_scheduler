#include "fcfs.h"
#include "sjf.h"
#include "hrrn.h"
#include "srtn.h"
#include "rr.h"
#include "priority.h"
#include "mlfq.h"
#include "simulator.h"
#include "testsets.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <filesystem>

std::vector<Process> makeTestSet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                 std::uint64_t burst, int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid;
    p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "P1", 0, 8, 3);
  add(2, "P2", 1, 4, 1);
  add(3, "P3", 2, 9, 4);
  add(4, "P4", 3, 5, 2);
  return procs;
}

std::vector<Process> makeIoTestSet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name,
                 std::uint64_t arrival, std::uint64_t burst,
                 int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid; p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "CPU1", 0, 20, 2);
  add(2, "IO1",  1,  2, 1, {{1, 5}, {1, 5}, {0, 0}});
  add(3, "IO2",  2,  2, 1, {{1, 5}, {1, 5}, {0, 0}});
  add(4, "CPU2", 3, 10, 3);
  return procs;
}

// Набор с несколькими I/O-блокировками (эффект конвоя): atTick возрастает и < burstTime
std::vector<Process> makeConvoySet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                 std::uint64_t burst, int priority,
                 std::vector<IoBlock> io = {}) {
    Process p;
    p.pid = pid; p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    p.ioBlocks = std::move(io);
    procs.push_back(p);
  };
  add(1, "CPU1", 0, 20, 2);
  add(2, "IO1", 1, 6, 1, {{1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}});
  add(3, "IO2", 2, 6, 1, {{1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}});
  add(4, "CPU2", 3, 12, 3);
  return procs;
}

// Набор для сравнения SJF и HRRN: один длинный процесс L и два коротких
std::vector<Process> makeHrrnSet() {
  std::vector<Process> procs;
  auto add = [&](int pid, const std::string& name, std::uint64_t arrival,
                 std::uint64_t burst, int priority) {
    Process p;
    p.pid = pid; p.name = name;
    p.arrivalTime = arrival;
    p.burstTime = burst;
    p.remainingTime = burst;
    p.priority = priority;
    p.dynamicPriority = priority;
    procs.push_back(p);
  };
  add(1, "A", 0, 4, 1);
  add(2, "L", 1, 6, 1);
  add(3, "S1", 4, 2, 1);
  add(4, "S2", 5, 1, 1);
  return procs;
}

void printResult(const SimResult& r) {
  std::cout << std::left << std::setw(30) << r.algorithm
            << " | wait=" << std::setw(7) << std::fixed << std::setprecision(2) << r.avgWaiting
            << " | turn=" << std::setw(7) << r.avgTurnaround
            << " | resp=" << std::setw(7) << r.avgResponse
            << " | CPU=" << std::setw(6) << r.cpuUtilization << "%"
            << " | CS=" << std::setw(4) << r.contextSwitches
            << " | done=" << r.throughput
            << "\n";
}

// Компактная диаграмма Ганта в одну строку: имя[начало-конец)
void printGanttLine(const SimResult& r, const std::vector<Process>& procs) {
  std::cout << r.algorithm << ": ";
  for (auto& [pid, span] : r.gantt) {
    if (pid == -1) std::cout << "idle";
    else if (pid == -2) std::cout << "CS";
    else for (auto& p : procs) if (p.pid == pid) std::cout << p.name;
    std::cout << "[" << span.first << "-" << span.second << ") ";
  }
  std::cout << "\n";
}

void printGantt(const SimResult& r) {
  std::cout << "Gantt (" << r.algorithm << "):\n";
  for (auto& [pid, span] : r.gantt) {
    std::cout << "  [" << span.first << "-" << span.second << ") ";
    if (pid == -1) std::cout << "IDLE\n";
    else if (pid == -2) std::cout << "CS\n";
    else std::cout << "P" << pid << "\n";
  }
  std::cout << "\n";
}

int main() {
  // папки для наборов и результатов (запускать из корня проекта)
  std::filesystem::create_directories("sets");
  std::filesystem::create_directories("results");
  std::cout << "Case 1: CPU-bound\n";
  {
    auto set1 = makeTestSet();
    std::vector<std::unique_ptr<Scheduler>> scheds;
    scheds.push_back(std::make_unique<FcfsScheduler>(set1));
    scheds.push_back(std::make_unique<SjfScheduler>(set1));
    scheds.push_back(std::make_unique<SrtnScheduler>(set1));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 1));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 2));
    scheds.push_back(std::make_unique<RrScheduler>(set1, 4));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, false, false));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, true, false));
    scheds.push_back(std::make_unique<PriorityScheduler>(set1, true, true));
    scheds.push_back(std::make_unique<MlfqScheduler>(set1));
    for (auto& s : scheds) printResult(runSimulation(*s));
  }

  std::cout << "\nCase 2. Mixed loading: I/O + CPU\n";
  {
    auto set2 = makeIoTestSet();
    std::vector<std::unique_ptr<Scheduler>> scheds;
    scheds.push_back(std::make_unique<FcfsScheduler>(set2));
    scheds.push_back(std::make_unique<SrtnScheduler>(set2));
    scheds.push_back(std::make_unique<RrScheduler>(set2, 4));
    scheds.push_back(std::make_unique<PriorityScheduler>(set2, true, true));
    scheds.push_back(std::make_unique<MlfqScheduler>(set2));
    for (auto& s : scheds) printResult(runSimulation(*s));
  }

  std::cout << "\n";
  {
    auto set = makeTestSet();
    RrScheduler rr(set, 2);
    SimResult r = runSimulation(rr);
    printGantt(r);
  }

  {
    auto set = makeTestSet();
    RrScheduler rr(set, 2);
    SimResult r = runSimulation(rr);
    std::cout << "\nПо процессам, RR (q=2):\n";
    printProcessTable(rr.processes());
  }

  {
    auto set = makeTestSet();
    saveSet("sets/set_basic.txt", set);
    std::vector<Process> loaded;
    if (!loadSet("sets/set_basic.txt", loaded)) {
      std::cerr << "Не удалось загрузить sets/set_basic.txt\n";
      return 1;
    }
    std::cout << "\nПроверка сохранения/загрузки (FCFS: исходный и загруженный набор):\n";
    FcfsScheduler a(set), b(loaded);
    printResult(runSimulation(a));
    printResult(runSimulation(b));  // строки должны совпасть
  }

  std::cout << "\nЗадание 3: SJF и HRRN на наборе A, L, S1, S2\n";
  {
    auto set = makeHrrnSet();
    SjfScheduler sjf(set);
    HrrnScheduler hrrn(set);
    SimResult a = runSimulation(sjf);
    SimResult b = runSimulation(hrrn);
    printGanttLine(a, sjf.processes());
    printGanttLine(b, hrrn.processes());
    printResult(a);
    printResult(b);
    printProcessTable(sjf.processes());
    printProcessTable(hrrn.processes());
  }

  std::cout << "\nЗадание 4: накладные расходы на переключение контекста (makeTestSet)\n";
  {
    auto set = makeTestSet();
    std::cout << "q  | wait(0) turn(0) resp(0) | wait(1) turn(1) resp(1) CPU%(1) overhead%(1)\n";
    for (std::uint64_t q : {1, 2, 4, 8}) {
      RrScheduler r0(set, q), r1(set, q);
      SimResult a = runSimulation(r0, 100000, 0);
      SimResult b = runSimulation(r1, 100000, 1);
      std::cout << std::left << std::setw(3) << q << "| " << std::fixed << std::setprecision(2)
                << std::setw(8) << a.avgWaiting << std::setw(8) << a.avgTurnaround << std::setw(8) << a.avgResponse << "| "
                << std::setw(8) << b.avgWaiting << std::setw(8) << b.avgTurnaround << std::setw(8) << b.avgResponse
                << std::setw(8) << b.cpuUtilization << std::setw(8) << b.overheadPercent << "\n";
    }
    std::cout << "\nFCFS и SJF с той же стоимостью переключения:\n";
    for (std::uint64_t cost : {0, 1}) {
      FcfsScheduler f(set);
      SjfScheduler s(set);
      std::cout << "cost=" << cost << ": ";
      printResult(runSimulation(f, 100000, cost));
      std::cout << "cost=" << cost << ": ";
      printResult(runSimulation(s, 100000, cost));
    }
    RrScheduler rr(set, 2);
    printGantt(runSimulation(rr, 100000, 1));
  }

  {
    auto set = makeTestSet();
    std::cout << "\nЗадание 5: влияние кванта в RR (набор makeTestSet)\n";
    std::cout << " q wait turn resp CS\n";
    for (std::uint64_t q : {1, 2, 4, 8, 16}) {
      RrScheduler rr(set, q);
      SimResult r = runSimulation(rr);
      std::cout << std::setw(2) << q << " " << std::fixed << std::setprecision(2)
                << r.avgWaiting << " " << r.avgTurnaround << " " << r.avgResponse
                << " " << std::setw(3) << r.contextSwitches << " "
                << std::string(r.contextSwitches, '#') << "\n";
    }
  }

  std::cout << "\nЗадание 6: набор с несколькими I/O-блокировками (makeConvoySet)\n";
  {
    auto set = makeConvoySet();
    std::vector<std::unique_ptr<Scheduler>> scheds;
    scheds.push_back(std::make_unique<FcfsScheduler>(set));
    scheds.push_back(std::make_unique<SrtnScheduler>(set));
    scheds.push_back(std::make_unique<RrScheduler>(set, 4));
    scheds.push_back(std::make_unique<PriorityScheduler>(set, true, true));
    scheds.push_back(std::make_unique<MlfqScheduler>(set));
    for (auto& s : scheds) {
      SimResult r = runSimulation(*s);
      printResult(r);
      printProcessTable(s->processes());
      if (r.algorithm == "FCFS" || r.algorithm == "MLFQ") printGanttLine(r, s->processes());
      std::cout << "\n";
    }
  }

  return 0;
}
