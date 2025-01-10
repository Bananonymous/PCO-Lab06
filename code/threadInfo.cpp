#include "threadInfo.h"
#include "threadpool.h"  // Now we have the full ThreadPool definition

ThreadInfo::ThreadInfo(std::string id, ThreadPool *p, std::chrono::milliseconds ms)
    : id(std::move(id)), pool(p), idleTimeout(ms), isRunning(false) {
}

ThreadInfo::~ThreadInfo() {
    if (thread) {
        isRunning = false;
        thread->join();
    }
}

void ThreadInfo::start() {
    isRunning = true;
    thread = std::make_unique<PcoThread>([this]() {
        workerTask();
    });
}

bool ThreadInfo::isThreadRunning() const {
    return isRunning;
}

void ThreadInfo::workerTask() {
    auto lastWorkTime = std::chrono::steady_clock::now();

    while (isRunning) {
        // Now we have a complete ThreadPool type, so this is valid:
        didWork = pool->taskRunner();

        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWorkTime) >= idleTimeout && !didWork) {
            // std::cout << "Thread " << id << " is idle for too long, exiting" << std::endl;
            isRunning = false;
            break;
        }
        // If a real task was run, update lastWorkTime...
        if (didWork)
            lastWorkTime = std::chrono::steady_clock::now();
        didWork = false;
    }
}
