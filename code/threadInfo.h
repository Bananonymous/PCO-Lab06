#ifndef THREADINFO_H
#define THREADINFO_H

#include <pcosynchro/pcothread.h>
#include <string>
#include <chrono>
#include <memory>
#include <atomic>

// Forward-declare ThreadPool only
class ThreadPool;

class ThreadInfo {
public:
    ThreadInfo(std::string id, ThreadPool* pool, std::chrono::milliseconds idleTimeout);
    ~ThreadInfo();

    void start();
    bool isThreadRunning() const;

private:
    void workerTask(); // declared but not defined here!

private:
    std::string id;
    ThreadPool* pool;
    bool didWork;
    std::unique_ptr<PcoThread> thread;
    std::chrono::milliseconds idleTimeout;
    std::atomic<bool> isRunning;
};

#endif
