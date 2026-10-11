#pragma once
#include <deque>
#include <algorithm>
#include "scheduler.h"

// Round-Robin
class RrScheduler : public Scheduler {
public:
  RrScheduler(std::vector<Process> processes, std::uint64_t quantum)
    : Scheduler(std::move(processes)), quantum_(quantum) {}

  std::string name() const override {
    return "RR (q=" + std::to_string(quantum_) + ")";
  }

  void onTick(std::uint64_t tick) override {
    for (auto& p : processes_) {
      if (p.state == ProcessState::NEW && p.arrivalTime <= tick) {
        p.state = ProcessState::READY;
        queue_.push_back(p.pid);
      }
    }
  }

  void onProcessReady(int pid, std::uint64_t) override {
    queue_.push_back(pid);
  }

  int pickNext(std::uint64_t) override {
    if (queue_.empty()) return -1;
    currentQuantumUsed_ = 0;
    return queue_.front();
  }

  void onProcessFinished(int pid, std::uint64_t) override {
    Process* p = find(pid);
    if (p) p->state = ProcessState::TERMINATED;
    auto it = std::find(queue_.begin(), queue_.end(), pid);
    if (it != queue_.end()) queue_.erase(it);
  }

  void onProcessBlocked(int pid, std::uint64_t) override {
    auto it = std::find(queue_.begin(), queue_.end(), pid);
    if (it != queue_.end()) queue_.erase(it);
  }

  void onProcessPreempted(int pid, std::uint64_t) override {
    // Квант истёк — процесс уже перемещён в конец в shouldPreempt
    (void)pid;
  }

  bool shouldPreempt(int currentPid, std::uint64_t) override {
    if (currentQuantumUsed_ >= static_cast<int>(quantum_)) {
      if (!queue_.empty() && queue_.front() == currentPid) {
        queue_.pop_front();
        queue_.push_back(currentPid);
      }
      return true;
    }
    return false;
  }

  void onProcessRanTick(int /*pid*/) override {
    currentQuantumUsed_++;
  }

private:
  std::uint64_t quantum_;
  std::deque<int> queue_;
  int currentQuantumUsed_ = 0;
};
