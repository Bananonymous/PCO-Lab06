/**
 * @file threadInfo.h
 * @brief Header file for the ThreadInfo class, representing individual threads in the thread pool.
 * @authors
 * - Nicolet Victor
 * - Surbeck Léon
 */

#ifndef THREADINFO_H
#define THREADINFO_H

#include <pcosynchro/pcothread.h>
#include <string>
#include <chrono>
#include <memory>
#include <atomic>

// Forward declaration of the ThreadPool class to avoid circular dependency
class ThreadPool;

/**
 * @brief ThreadInfo class
 * Represents an individual thread in the thread pool, capable of fetching and executing tasks.
 */
class ThreadInfo {
public:
    /**
     * @brief Constructor
     * @param id A unique identifier for the thread
     * @param pool A pointer to the thread pool managing this thread
     * @param idleTimeout The duration after which an idle thread terminates
     */
    ThreadInfo(std::string id, ThreadPool* pool, std::chrono::milliseconds idleTimeout);

    /**
     * @brief Destructor
     * Ensures the thread is properly joined before destruction.
     */
    ~ThreadInfo();

    /**
     * @brief Starts the thread and assigns it to the task execution loop.
     */
    void start();

    /**
     * @brief Checks if the thread is currently running.
     * @return True if the thread is running, false otherwise.
     */
    bool isThreadRunning() const;

private:
    /**
     * @brief The main task execution loop for the thread.
     * Continuously fetches and executes tasks from the thread pool.
     */
    void workerTask();

private:
    std::string id;                          // Unique identifier for the thread
    ThreadPool* pool;                        // Pointer to the parent thread pool
    bool didWork;                            // Indicates if the thread performed a task
    std::unique_ptr<PcoThread> thread;       // Pointer to the thread object
    std::chrono::milliseconds idleTimeout;   // Duration before the thread terminates if idle
    std::atomic<bool> isRunning;             // Atomic flag indicating if the thread is running
};

#endif // THREADINFO_H
