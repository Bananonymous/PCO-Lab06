/**
 * @file threadInfo.cpp
 * @brief Implementation of the ThreadInfo class, which encapsulates individual threads in the thread pool.
 * @authors
 * - Nicolet Victor
 * - Surbeck Léon
 */

#include "threadInfo.h"
#include "threadpool.h"

/**
 * @brief Constructor for ThreadInfo
 * @param id A unique string identifier for the thread
 * @param pool A pointer to the thread pool managing this thread
 * @param ms Idle timeout for the thread
 */
ThreadInfo::ThreadInfo(std::string id, ThreadPool* p, std::chrono::milliseconds ms)
    : id(std::move(id)), pool(p), idleTimeout(ms), isRunning(false) {}

/**
 * @brief Destructor for ThreadInfo
 * Ensures the thread is joined before destruction.
 */
ThreadInfo::~ThreadInfo() {
    if (thread) {
        isRunning = false; // Signal the thread to stop
        thread->join();    // Wait for thread completion
    }
}

/**
 * @brief Starts the thread and begins processing tasks.
 */
void ThreadInfo::start() {
    isRunning = true;
    thread = std::make_unique<PcoThread>([this]() {
        workerTask();
    });
}

/**
 * @brief Checks if the thread is currently running.
 * @return True if the thread is running, false otherwise.
 */
bool ThreadInfo::isThreadRunning() const {
    return isRunning;
}

/**
 * @brief The main worker task for the thread.
 * Continuously fetches and executes tasks from the thread pool.
 */
void ThreadInfo::workerTask() {
    auto lastWorkTime = std::chrono::steady_clock::now(); // Last time a task was executed

    while (isRunning) {
        // Fetch and execute a task from the thread pool
        didWork = pool->taskRunner();

        auto now = std::chrono::steady_clock::now();

        // Exit if the thread has been idle for too long
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWorkTime) >= idleTimeout && !didWork) {
            isRunning = false;
            break;
        }

        // Update the last work time if a task was executed
        if (didWork)
            lastWorkTime = std::chrono::steady_clock::now();

        didWork = false; // Reset the work flag
    }
}
