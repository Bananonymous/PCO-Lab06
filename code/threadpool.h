/**
 * @file threadpool.h
 * @brief Header file defining the ThreadPool class, which manages a pool of worker threads to execute tasks.
 * @authors
 * - Nicolet Victor
 * - Surbeck Léon
 */

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

/**
 * @brief Abstract base class representing a runnable task.
 * Defines the interface for tasks that can be executed by the ThreadPool.
 */
class Runnable {
public:
    virtual ~Runnable() = default;

    /**
     * @brief Execute the task.
     */
    virtual void run() = 0;

    /**
     * @brief Cancel the task.
     * Called when the task cannot be executed (e.g., due to queue overflow).
     */
    virtual void cancelRun() = 0;

    /**
     * @brief Get the unique identifier for the task.
     * @return A string representing the task ID.
     */
    virtual std::string id() = 0;
};

/**
 * @brief The ThreadPool class
 * Manages a fixed number of worker threads to execute submitted tasks concurrently.
 */
class ThreadPool : public PcoHoareMonitor {
public:
    /**
     * @brief Constructor
     * @param maxThreadCount Maximum number of worker threads in the pool
     * @param maxNbWaiting Maximum number of tasks allowed in the queue
     * @param idleTimeout Duration before idle threads terminate
     * @throws std::invalid_argument if parameters are invalid
     */
    ThreadPool(int maxThreadCount, int maxNbWaiting, std::chrono::milliseconds idleTimeout)
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

    /**
     * @brief Destructor
     * Gracefully shuts down the thread pool and releases resources.
     */
    ~ThreadPool() {
        shutdown();
    }

    /**
     * @brief Fetches and executes a task from the queue.
     * @return True if a task was executed, false otherwise.
     */
    bool taskRunner() {
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

            try {
                if (task) task->run();
                return true;
            } catch (const std::exception& e) {
                logger() << "[ERROR] Task execution failed: " << e.what() << "\n";
                return false;
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

    /**
     * @brief Starts a runnable task.
     * @param runnable The task to be executed.
     * @return True if the task was successfully started, false otherwise.
     */
    bool start(std::unique_ptr<Runnable> runnable) {
        monitorIn();

        // If shutting down, refuse new tasks
        if (isShuttingDown || maxThreadCount < 0) {
            monitorOut();
            return false;
        }

        // Add this new task to the queue
        waitingTasks.push(std::move(runnable));

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
            waitingTasks.front()->cancelRun();
            waitingTasks.pop();
            monitorOut();
            return false;
        }

        monitorOut();
        return true;
    }

    /**
     * @brief Returns the number of currently running threads.
     * @return The count of running threads.
     */
    size_t currentNbThreads() {
        return std::count_if(threadPool.begin(), threadPool.end(),
                             [](const auto &threadInfo) {
                                 return threadInfo->isThreadRunning();
                             });
    }

private:
    size_t maxThreadCount;                      // Maximum number of worker threads
    size_t maxNbWaiting;                        // Maximum number of waiting tasks in the queue
    size_t idleThreads;                         // Current number of idle threads
    std::chrono::milliseconds idleTimeout;      // Duration before idle threads terminate
    size_t nbTasksWaitingForThreads = 0;        // Number of tasks waiting for threads
    std::queue<std::unique_ptr<Runnable>> waitingTasks; // Queue of waiting tasks
    std::vector<std::unique_ptr<ThreadInfo>> threadPool; // Pool of worker threads
    Condition condTaskAvailable;                // Condition for available tasks
    Condition waitForThread;                    // Condition for waiting threads
    bool isShuttingDown;                        // Indicates if the pool is shutting down

    /**
     * @brief Creates a new worker thread in the pool.
     */
    void createThread() {
        static int threadCounter = 0;
        auto threadId = "Thread-" + std::to_string(++threadCounter);

        // Create a new thread and start its execution loop
        threadPool.emplace_back(std::make_unique<ThreadInfo>(
            threadId,
            this, // 'this' is the pointer to the current ThreadPool
            idleTimeout
        ));
        threadPool.back()->start();
    }

    /**
     * @brief Gracefully shuts down the thread pool.
     */
    void shutdown() {
        isShuttingDown = true;

        // Wake up all threads so they can exit
        for (auto &thread : threadPool) {
            signal(condTaskAvailable);
            signal(waitForThread);
        }

        // Clear all threads and join them
        threadPool.clear();

        // Clear out any remaining tasks
        while (!waitingTasks.empty()) {
            waitingTasks.pop();
        }
    }
};

#endif // THREADPOOL_H
