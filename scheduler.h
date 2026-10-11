#pragma once

#include "process.h"
#include <vector>
#include <string>
#include <cstdint>

// Базовый интерфейс планировщика
class Scheduler {
public:
  explicit Scheduler(std::vector<Process> processes)
    : processes_(std::move(processes)) {
    for (auto& p : processes_) {
      p.remainingTime = p.burstTime;
      p.dynamicPriority = p.priority;
    }
  }
  virtual ~Scheduler() = default;

  virtual std::string name() const = 0;

  // Вызывается в начале каждого такта
  virtual void onTick(std::uint64_t tick) = 0;

  // Выбор следующего процесса (-1, если нет готовых)
  virtual int pickNext(std::uint64_t tick) = 0;

  // Уведомления о событиях
  virtual void onProcessFinished(int /*pid*/, std::uint64_t /*tick*/) {}
  virtual void onProcessBlocked(int /*pid*/, std::uint64_t /*tick*/) {}
  virtual void onProcessPreempted(int /*pid*/, std::uint64_t /*tick*/) {}
  virtual void onProcessReady(int /*pid*/, std::uint64_t /*tick*/) {}
  virtual void onProcessRanTick(int /*pid*/) {}

  // Нужно ли вытеснить текущий процесс
  virtual bool shouldPreempt(int currentPid, std::uint64_t tick) = 0;

  // Доступ к процессам
  std::vector<Process>& processes() { return processes_; }
  const std::vector<Process>& processes() const { return processes_; }

  Process* find(int pid) {
    for (auto& p : processes_) if (p.pid == pid) return &p;
    return nullptr;
  }

protected:
  std::vector<Process> processes_;
};
