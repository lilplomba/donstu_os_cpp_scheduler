#include "simulator.h"

SimResult runSimulationMulti(Scheduler& sched, int cores, std::uint64_t maxTicks) {
  SimResult res;
  res.algorithm = sched.name() + " x" + std::to_string(cores);
  res.coreGantt.resize(cores);

  std::vector<int> current(cores, -1);  // кто сейчас на каждом ядре
  std::vector<int> prev(cores, -1);     // кто выполнялся на ядре в прошлом такте
  std::uint64_t busyTicks = 0;          // суммарно по всем ядрам
  std::uint64_t tick = 0;

  auto& procs = sched.processes();

  while (tick < maxTicks) {
    // 1. Возвращаем процессы из I/O
    for (auto& p : procs) {
      if (p.state == ProcessState::WAITING && p.ioReturnTick <= tick) {
        p.state = ProcessState::READY;
        sched.onProcessReady(p.pid, tick);
      }
    }

    // 2. Все ли завершены?
    bool allDone = true;
    for (auto& p : procs) {
      if (p.state != ProcessState::TERMINATED) { allDone = false; break; }
    }
    if (allDone) break;

    // 3. Планировщик обновляет очереди — один раз за такт, а не по разу на ядро
    sched.onTick(tick);

    // 4. Вытеснение: спрашиваем по каждому ядру, у которого есть процесс
    for (int c = 0; c < cores; ++c) {
      if (current[c] != -1 && sched.shouldPreempt(current[c], tick)) {
        Process* p = sched.find(current[c]);
        if (p && !p->isFinished() && p->state == ProcessState::RUNNING) {
          p->state = ProcessState::READY;
          sched.onProcessPreempted(current[c], tick);
        }
        current[c] = -1;
      }
    }

    // 5. Каждое свободное ядро (по очереди) выбирает процесс из общей очереди
    for (int c = 0; c < cores; ++c) {
      if (current[c] != -1) continue;
      current[c] = sched.pickNext(tick);
      if (current[c] == -1) continue;
      Process* p = sched.find(current[c]);
      if (!p) continue;
      if (!p->started) {
        p->started = true;
        p->startTime = tick;
        p->responseTime = tick - p->arrivalTime;
      }
      p->state = ProcessState::RUNNING;
      p->contextSwitches++;
      if (prev[c] != current[c]) res.contextSwitches++;
    }

    // 6. Каждое ядро выполняет свой такт
    std::vector<int> ran(cores, -1);
    for (int c = 0; c < cores; ++c) {
      if (current[c] == -1) continue;
      Process* p = sched.find(current[c]);
      if (!p) continue;
      ran[c] = current[c];
      p->remainingTime--;
      p->executedTicks++;
      busyTicks++;
      sched.onProcessRanTick(current[c]);

      if (p->nextIoIndex < p->ioBlocks.size() &&
          p->executedTicks == p->ioBlocks[p->nextIoIndex].atTick) {
        p->state = ProcessState::WAITING;
        p->ioReturnTick = tick + 1 + p->ioBlocks[p->nextIoIndex].duration;
        p->ioWaitTime += p->ioBlocks[p->nextIoIndex].duration;
        p->nextIoIndex++;
        sched.onProcessBlocked(current[c], tick);
        current[c] = -1;
      } else if (p->isFinished()) {
        p->finishTime = tick + 1;
        p->turnaroundTime = p->finishTime - p->arrivalTime;
        sched.onProcessFinished(current[c], tick);
        current[c] = -1;
      }
    }

    // 7. +1 к waitingTime у всех, кто остался в READY (один раз, не по разу на ядро)
    for (auto& p : procs) {
      if (p.state == ProcessState::READY) p.waitingTime++;
    }

    // 8. Диаграмма Ганта для каждого ядра
    // (до этого такта работа ещё была — шаг 2 не вышел из цикла, поэтому простой пишем всегда)
    for (int c = 0; c < cores; ++c) {
      auto& g = res.coreGantt[c];
      int pid = ran[c];
      if (!g.empty() && g.back().first == pid) {
        g.back().second.second = tick + 1;
      } else {
        g.push_back({pid, {tick, tick + 1}});
      }
    }

    prev = ran;
    tick++;
  }

  // Метрики
  double sumW = 0, sumT = 0, sumR = 0;
  int finished = 0;
  for (auto& p : procs) {
    if (p.state == ProcessState::TERMINATED) {
      sumW += p.waitingTime;
      sumT += p.turnaroundTime;
      sumR += p.responseTime;
      finished++;
    }
  }
  if (finished > 0) {
    res.avgWaiting = sumW / finished;
    res.avgTurnaround = sumT / finished;
    res.avgResponse = sumR / finished;
  }
  res.totalTicks = tick;
  res.cpuUtilization = tick > 0 ? 100.0 * busyTicks / (tick * cores) : 0.0;
  res.throughput = finished;
  return res;
}
