#pragma once
#include "scheduler.h"
#include <algorithm>
#include <vector>
#include <unordered_map>

// Приоритетное планирование с опциональным старением
class PriorityScheduler : public Scheduler {
public:
  PriorityScheduler(std::vector<Process> processes,
                    bool preemptive, bool aging)
    : Scheduler(std::move(processes)),
    preemptive_(preemptive), aging_(aging) {}

  std::string name() const override {
    std::string s = "Priority";
    if (preemptive_) s += " (preemptive)";
    if (aging_) s += " + aging";
    return s;
  }

  void onTick(std::uint64_t tick) override {
    for (auto& p : processes_) {
      if (p.state == ProcessState::NEW && p.arrivalTime <= tick) {
        p.state = ProcessState::READY;
        p.dynamicPriority = p.priority;
        ready_.push_back(p.pid);
        waitingSince_[p.pid] = tick;
      }
    }
    // Старение: каждые 10 тактов ожидания повышаем приоритет
    if (aging_) {
      for (int pid : ready_) {
        Process* p = find(pid);
        if (p) {
          auto it = waitingSince_.find(pid);
          std::uint64_t start = (it != waitingSince_.end()) ? it->second : tick;
          std::uint64_t waited = tick - start;
          p->dynamicPriority = std::max(0, p->priority - static_cast<int>(waited / 10));
        }
      }
    }
  }

  void onProcessReady(int pid, std::uint64_t tick) override {
    Process* p = find(pid);
    // сброс после I/O
    if (p) p->dynamicPriority = p->priority;
    ready_.push_back(pid);
    waitingSince_[pid] = tick;
  }

  int pickNext(std::uint64_t) override {
    if (ready_.empty()) return -1;
    auto it = std::min_element(ready_.begin(), ready_.end(), [&](int a, int b) {
      return find(a)->dynamicPriority < find(b)->dynamicPriority;
    });
    int pid = *it;
    ready_.erase(it);
    return pid;
  }

  void onProcessFinished(int pid, std::uint64_t) override {
    Process* p = find(pid);
    if (p) p->state = ProcessState::TERMINATED;
    auto it = std::find(ready_.begin(), ready_.end(), pid);
    if (it != ready_.end()) ready_.erase(it);
  }

  void onProcessBlocked(int /*pid*/, std::uint64_t) override {
    // вернётся через onProcessReady
  }

  void onProcessPreempted(int pid, std::uint64_t tick) override {
    ready_.push_back(pid);
    waitingSince_[pid] = tick;
  }

  bool shouldPreempt(int currentPid, std::uint64_t) override {
    if (!preemptive_) return false;
    Process* cur = find(currentPid);
    if (!cur || cur->isFinished()) return false;
    for (int pid : ready_) {
      Process* p = find(pid);
      if (p && p->dynamicPriority < cur->dynamicPriority) return true;
    }
    return false;
  }

private:
  bool preemptive_;
  bool aging_;
  std::vector<int> ready_;
  // по pid
  std::unordered_map<int, std::uint64_t> waitingSince_;
};
