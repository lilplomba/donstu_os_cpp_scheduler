#pragma once
#include "scheduler.h"
#include <algorithm>
#include <vector>

// Shortest Job First (невытесняющий)
class SjfScheduler : public Scheduler {
public:
  using Scheduler::Scheduler;

  std::string name() const override { return "SJF"; }

  void onTick(std::uint64_t tick) override {
    for (auto& p : processes_) {
      if (p.state == ProcessState::NEW && p.arrivalTime <= tick) {
        p.state = ProcessState::READY;
        ready_.push_back(p.pid);
      }
    }
  }

  void onProcessReady(int pid, std::uint64_t) override {
    ready_.push_back(pid);
  }

  int pickNext(std::uint64_t) override {
    if (ready_.empty()) return -1;
    auto it = std::min_element(ready_.begin(), ready_.end(),
                               [&](int a, int b) {
                                 return find(a)->burstTime < find(b)->burstTime;
                               });
    int pid = *it;
    ready_.erase(it);
    return pid;
  }

  void onProcessFinished(int pid, std::uint64_t) override {
    Process* p = find(pid);
    if (p) p->state = ProcessState::TERMINATED;
  }

  void onProcessBlocked(int /*pid*/, std::uint64_t) override {
    // Невытесняющий: если процесс ушёл в I/O, он вернётся через onProcessReady
  }

  void onProcessPreempted(int pid, std::uint64_t) override {
    ready_.push_back(pid);  // на всякий случай
  }

  bool shouldPreempt(int, std::uint64_t) override { return false; }

private:
  std::vector<int> ready_;
};
