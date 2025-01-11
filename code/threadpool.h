#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <algorithm>
#include <iostream>
#include <queue>
#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include <cassert>
#include <functional>

#include <pcosynchro/pcohoaremonitor.h>

#include "threadInfo.h" // Include the ThreadInfo class

class Runnable {
public:
    virtual ~Runnable() = default;

    virtual void run() = 0;
    virtual void cancelRun() = 0;
    virtual std::string id() = 0;
};

class ThreadPool : public PcoHoareMonitor {
public:
    ThreadPool(int maxThreadCount,
               int maxNbWaiting,
               std::chrono::milliseconds idleTimeout)
        : maxThreadCount(maxThreadCount),
          maxNbWaiting(maxNbWaiting),
          idleThreads(0),
          idleTimeout(idleTimeout),
          isShuttingDown(false) {
        if (maxThreadCount <= 0) {
            throw std::invalid_argument("maxThreadCount must be greater than 0");
        }
        if (maxNbWaiting < 0) {
            throw std::invalid_argument("maxNbWaiting cannot be negative");
        }
        if (idleTimeout.count() < 0) {
            throw std::invalid_argument("idleTimeout must be non-negative");
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // The main function used by a worker to fetch and run tasks
    bool taskRunner()
    {
        monitorIn();

        if (isShuttingDown) {
            monitorOut();
            return false;
        }

        // If there is at least one waiting task, pop and run it outside the monitor
        if (!waitingTasks.empty()) {
            auto task = std::move(waitingTasks.front());
            waitingTasks.pop();
            monitorOut(); // Release the monitor while running a task

            if (task) {
                task->run();
                return true;
            }
        } else {
            // No tasks -> the thread becomes idle
            ++idleThreads;
            if (nbTasksWaitingForThreads > 0) {
                signal(waitForThread);
            }
            monitorOut();
        }
        return false;
    }

    // Start a runnable task
    bool start(std::unique_ptr<Runnable> runnable)
    {
        monitorIn();

        // If shutting down, refuse new tasks
        if (isShuttingDown || maxThreadCount < 0) {
            monitorOut();
            return false;
        }

        // Add this new task to the queue
        waitingTasks.push(std::move(runnable));

        // std::cout << currentNbThreads() << " / " << maxThreadCount << " / " << idleThreads << " / " << waitingTasks.size() <<  std::endl;

        // If we have idle threads, wake one up
        if (idleThreads > 0) {
            signal(condTaskAvailable);
        }
        // Otherwise, if we still can create new threads, do it
        else if (currentNbThreads() < maxThreadCount) {
            createThread();
        }
        // Else if we haven't exceeded the queue limit, wait to see if a thread becomes available
        else if (waitingTasks.size() - 1 < maxNbWaiting) {
            ++nbTasksWaitingForThreads;
            wait(waitForThread);
            --nbTasksWaitingForThreads;
        }
        // Otherwise, we've exceeded our limit and remove the task
        else {
            // std::cout << "Trashing task" << std::endl;
            waitingTasks.front()->cancelRun();
            waitingTasks.pop();
            monitorOut();
            return false;
        }

        monitorOut();
        return true;
    }

    // Returns the number of currently running threads
    size_t currentNbThreads()
    {
        return std::count_if(threadPool.begin(), threadPool.end(),
                             [](const auto &threadInfo) {
                                 return threadInfo->isThreadRunning();
                             });
    }

private:
    size_t maxThreadCount;
    size_t maxNbWaiting;
    size_t idleThreads;
    std::chrono::milliseconds idleTimeout;

    size_t nbTasksWaitingForThreads;

    std::queue<std::unique_ptr<Runnable>> waitingTasks;
    std::vector<std::unique_ptr<ThreadInfo>> threadPool;

    Condition condTaskAvailable;
    Condition waitForThread;
    bool isShuttingDown;

    // Create a new thread in the pool
    void createThread()
    {
        static int threadCounter = 0;
        auto threadId = "Thread-" + std::to_string(++threadCounter);

        // Pass `this` as a raw pointer to the ThreadInfo so it can call pool->taskFetcher()
        threadPool.emplace_back(std::make_unique<ThreadInfo>(
            threadId,
            this, // 'this' is the pointer to the current ThreadPool
            idleTimeout
        ));
        threadPool.back()->start();
    }

    // Gracefully shuts down the thread pool
    void shutdown()
    {
        isShuttingDown = true;

        // Wake up all threads so they can exit
        for (auto &thread : threadPool) {
            signal(condTaskAvailable);
            signal(waitForThread);
        }

        // Once we leave the monitor, the ~ThreadInfo will join worker threads
        // when we clear the vector
        threadPool.clear();

        // Clear out any remaining tasks
        while (!waitingTasks.empty()) {
            waitingTasks.pop();
        }

    }
};

#endif // THREADPOOL_H
