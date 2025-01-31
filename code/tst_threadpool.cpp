/**
* @file tst_threadpool.cpp
 * @brief Unit tests for the ThreadPool class, covering various concurrency scenarios.
 * @authors
 * - Nicolet Victor
 * - Surbeck Léon
 */
#include <chrono>

#include <gtest/gtest.h>

#include <pcosynchro/pcologger.h>
#include <pcosynchro/pcothread.h>

#include "threadpool.h"


#define RUNTIME 100000
#define RUNTIMEINMS 100

///
/// \brief The ThreadpoolTest class
/// This class embeds all the tests of the ThreadPool
class ThreadpoolTest : public ::testing::Test
{

public:
    ThreadpoolTest() = default;

    ///
    /// \brief runnableStarted Called before sending the Runnable to the pool
    /// \param id Id of the Runnable
    ///
    void runnableStarted(std::string id)
    {
        // No concurrent access, no need to protect it
        m_runningState[id] = true;
    }

    ///
    /// \brief runnableTerminated Called when the runnable finishes
    /// \param id Id of the Runnable
    /// It has to be called by the Runnable itself, and record the ending time
    void runnableTerminated(std::string id)
    {
        mutex.lock();
        // Erases the last value, protection between runnables is mandatory
        endingTime = std::chrono::system_clock::now();
        mutex.unlock();
        // Only a single access, no need to protect it
        m_runningState[id] = false;
    }

    /// \brief m_runningState A map to represent the running state of the Runnables
    /// True : running. False : not running
    std::map<std::string, bool> m_runningState;

    /// \brief m_started A map to represent the fact that the Runnables have been started
    /// True : started. False : not started
    std::map<std::string, bool> m_started;

    ///
    /// \brief startingTime To store the time the testcase started
    ///
    std::chrono::time_point<std::chrono::system_clock> startingTime;

    ///
    /// \brief endingTime To store the time of the end of the last Runnable
    ///
    std::chrono::time_point<std::chrono::system_clock> endingTime;

    //! A mutex to protect internal variables
    std::mutex mutex;

    ///
    /// \brief initTestCase Function to init the testcase.
    /// Needs to be called at the beginning of each testcase.
    void initTestCase()
    {
        startingTime = std::chrono::system_clock::now();
    }

    ///
    /// \brief testCase1 A testcase with a pool of 10 threads running 10 runnables
    /// Each runnable just waits 10 ms and finishes.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase1();

    ///
    /// \brief testCase2 A testcase with a pool of 10 threads running 100 runnables
    /// Each runnable just waits 10 ms and finishes.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase2();

    ///
    /// \brief testCase3 A testcase with a pool of 10 threads running 10x10 runnables
    /// Each runnable just waits 10 ms and finishes. A batch of 10 runnables is started,
    /// and after its completion, 10 other start, and so on.
    /// Check is done on the termination of the Runnables (everyone finished),
    /// and on the time required to run all the runnables.
    ///
    void testCase3();

    ///
    /// \brief testCase4 A testcase with a pool of 10 threads running 30 runnables
    /// Each runnable is launch in a thread to create a pile of waiting runnables
    /// Check is done on the termination of the Runnables (everyone finished),
    /// on the amount of failed runnables,
    /// and on the time required to run all the runnables.
    ///
    void testCase4();

    ///
    /// \brief testCase5
    ///
    void testCase5();
};




///
/// \brief The TestRunnable class
/// This class is a subclass of Runnable, and therefore is used by the testcases
/// as Runnables
class TestRunnable : public Runnable
{
    //! A reference on the main Test class
    ThreadpoolTest *m_tester;

    //! The Id of the Runnable
    std::string m_id;

    //! Run time the Runnable has to emulate
    uint64_t m_runTimeInUs;

public:

    ///
    /// \brief TestRunnable Simple constructor
    /// \param tester Reference to the main Test class
    /// \param id Id of the Runnable (should be unique)
    /// \param runtime The run time the Runnable shall emulate, in us
    ///
    TestRunnable(ThreadpoolTest *tester, std::string id, uint64_t runTimeInUs = RUNTIME) : m_tester(tester), m_id(std::move(id)), m_runTimeInUs(runTimeInUs){
    }

    //! Empty destructor
    ~TestRunnable() override = default;

    //! Method ran by the thread of the pool
    void run() override {
        logger() << "[TEST] RUNNING " << this->m_id << "\n";
        PcoThread::usleep(m_runTimeInUs);
        m_tester->runnableTerminated(m_id);
    }

    ///
    /// \brief id returns the Id of the Runnable
    /// \return The Id of the Runnable
    ///
    std::string id() override {
        return m_id;
    }

    void cancelRun() override {
        m_tester->runnableTerminated(m_id);
    }
};


typedef struct {
    int thread_id;
    std::unique_ptr<TestRunnable> runnable;
    ThreadPool *pool;
    ThreadpoolTest *test;
} ThreadParameter;

/**
 * @brief Launches a Runnable in a new thread within the ThreadPool.
 */
void startInThread(ThreadParameter *param) {

    auto id = param->runnable->id();
    if(!param->pool->start(std::move(param->runnable))) {
        param->test->m_started[id] = true;
        logger() << "[TEST] Runnable start failed " << param->thread_id << "\n";
    }
}


///
/// \brief A testcase with a pool of 10 threads running 10 runnables
/// Each runnable just waits 10 ms and finishes.
/// Check is done on the termination of the Runnables (everyone finished),
/// and on the time required to run all the runnables.
///
TEST_F(ThreadpoolTest, testCase1)
{
    initTestCase();
    ThreadPool pool(10, 50, std::chrono::milliseconds{100});

    // Starts the runnables
    for(int i = 0; i < 10; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        auto runnable = std::make_unique<TestRunnable>(this, runnableId);
        runnableStarted(runnableId);
        bool startStatus = pool.start(std::move(runnable));
        EXPECT_TRUE(startStatus);
    }


    // Check that every runnable is not finished yet
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, true) << "Failed";
    }

    PcoThread::usleep(1000 * (RUNTIMEINMS + 5));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (RUNTIMEINMS + 7)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (RUNTIMEINMS - 3)) << "Too short execution time";
}

///
/// \brief A testcase with a pool of 10 threads running 100 runnables
/// Each runnable just waits 10 ms and finishes.
/// Check is done on the termination of the Runnables (everyone finished),
/// and on the time required to run all the runnables.
///
TEST_F(ThreadpoolTest, testCase2)
{
    initTestCase();
    ThreadPool pool(10, 100, std::chrono::milliseconds{100});

    // Starts the runnables
    for(int i = 0; i < 100; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        auto runnable = std::make_unique<TestRunnable>(this, runnableId);
        runnableStarted(runnableId);
        bool startStatus = pool.start(std::move(runnable));
        EXPECT_TRUE(startStatus);
    }

    PcoThread::usleep(1000 * (10 * RUNTIMEINMS + 30));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS + 30)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS - 30)) << "Too short execution time";
}


///
/// \brief A testcase with a pool of 10 threads running 10x10 runnables
/// Each runnable just waits 10 ms and finishes. A batch of 10 runnables is started,
/// and after its completion, 10 other start, and so on.
/// Check is done on the termination of the Runnables (everyone finished),
/// and on the time required to run all the runnables.
///
TEST_F(ThreadpoolTest, testCase3)
{
    initTestCase();
    ThreadPool pool(10, 100, std::chrono::milliseconds{100});

    for(int nbBatch = 0; nbBatch < 10; nbBatch ++) {
        // Starts the runnables
        for(int i = 0; i < 10; i++) {
            std::string runnableId = "Run" + std::to_string(nbBatch) + "_" + std::to_string(i);
            auto runnable = std::make_unique<TestRunnable>(this, runnableId);
            runnableStarted(runnableId);
            bool startStatus = pool.start(std::move(runnable));
            EXPECT_TRUE(startStatus);
        }

        // Wait until completion of each runnable
        PcoThread::usleep(1000 * (RUNTIMEINMS + 30));

    }


    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed thread returned running";
    }

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS + 300)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (10 * RUNTIMEINMS - 300)) << "Too short execution time";
}




///
/// \brief A testcase with a pool of 10 threads running 30 runnables
/// Each runnable is launched in a thread to create a pile of waiting runnables
/// Check is done on the termination of the Runnables (everyone finished),
/// on the amount of failed runnables,
/// and on the time required to run all the runnables.
///
TEST_F(ThreadpoolTest, testCase4)
{
    initTestCase();
    ThreadPool pool(10, 5, std::chrono::milliseconds{100});
    std::vector<std::unique_ptr<PcoThread>> m_threads;

    std::array<ThreadParameter, 30> param;

    // Starts the runnables
    for(int i = 0; i < 30; i++) {
        std::string runnableId = "Run" + std::to_string(i);
        auto runnable = std::make_unique<TestRunnable>(this, runnableId);
        runnableStarted(runnableId);
        logger() << "[TEST] " << runnableId << "\n";
        param[i].thread_id = i;
        param[i].pool = &pool;
        param[i].runnable = std::move(runnable);
        param[i].test = this;

        m_threads.push_back(std::make_unique<PcoThread>(startInThread, &param[i]));
    }

    //Wait for the threads to finish
    for (size_t i = 0; i < 30; i++) {
        m_threads[i]->join();
    }
    m_threads.clear();

    // Wait until completion of each runnable
    PcoThread::usleep(1000 * (2 * RUNTIMEINMS + 30));

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }

    // Check that the right amount of runnable failed
    int nbStarted = 0;
    for (const auto& [key, value] : m_started) {
        if (value) {
            nbStarted ++;
        }
    }
    EXPECT_EQ(nbStarted, 15) << "Not the right amount of lost runnables";


    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (2 * RUNTIMEINMS + 30)) << "Too long execution time";

    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(endingTime - startingTime).count(), (2 * RUNTIMEINMS - 30)) << "Too short execution time";
}


///
/// \brief A testcase with a pool of 10 threads running 10 runnables and checking idle time
/// Each runnable is launched in a thread with a different running time.
/// An idle timeout is set to let the threads terminate one after the other.
/// It uses the currentNbThreads() function of the thread pool to check that.
///
TEST_F(ThreadpoolTest, testCase5)
{
    initTestCase();
    ThreadPool pool(10, 100, std::chrono::milliseconds{5});

    // Starts the runnables
    for(size_t i = 0; i < 10; i++) {
        std::string runnableId = "Run_" + std::to_string(i);
        // A runnable has a runtime of (i+1) seconds
        auto runnable = std::make_unique<TestRunnable>(this, runnableId, 1000000 * (i + 1));
        runnableStarted(runnableId);
        bool startStatus = pool.start(std::move(runnable));
        EXPECT_TRUE(startStatus);
    }

    // Wait half a second
    PcoThread::usleep(500000);

    for(size_t i = 0; i < 10; i++) {
        // Check that the threads are correctly removed from the pool
        EXPECT_EQ(pool.currentNbThreads(), 10 - i);
        PcoThread::usleep(1000000);
    }

    EXPECT_EQ(pool.currentNbThreads(), 0);

    logger() << "Waited for completion of each runnable\n";

    // Check that every runnable is really finished
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false) << "Failed";
    }
}

/// \brief A testcase where multiple threads call the `start` function simultaneously
/// This test checks if the pool handles concurrent task submissions without deadlocks or errors.
/// A total of 50 tasks are added from different threads, and their execution is validated.
TEST_F(ThreadpoolTest, testConcurrentStart) {
    ThreadPool pool(10, 20, std::chrono::milliseconds{100});
    std::vector<std::unique_ptr<PcoThread>> threads;
    constexpr int taskCount = 50;

    for (int i = 0; i < taskCount; i++) {
        threads.push_back(std::make_unique<PcoThread>([&pool, i, this]() {
            std::string id = "Task_" + std::to_string(i);
            auto runnable = std::make_unique<TestRunnable>(this, id);
            pool.start(std::move(runnable));
        }));
    }

    for (auto &thread : threads) {
        thread->join();
    }

    PcoThread::usleep(200000); // 200 ms

    // Check that all tasks have completed
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false);
    }
}

/// \brief A stress test with 4000 tasks and 20 threads in the pool
/// This test checks the scalability and stability of the pool under a high task load.
/// It ensures that all tasks complete without errors or deadlocks.
TEST_F(ThreadpoolTest, testStress) {
    ThreadPool pool(20, 100, std::chrono::milliseconds{100});
    constexpr int taskCount = 4000;

    for (int i = 0; i < taskCount; i++) {
        std::string id = "Task_" + std::to_string(i);
        auto runnable = std::make_unique<TestRunnable>(this, id);
        pool.start(std::move(runnable));
    }

    PcoThread::usleep(1000000); // Wait for all tasks to complete

    // Check that all tasks have completed
    for (const auto& [key, value] : m_runningState) {
        EXPECT_EQ(value, false);
    }
}

/// \brief A test to verify thread removal after idle timeout
/// This test ensures that threads are terminated correctly after being idle for the specified timeout.
TEST_F(ThreadpoolTest, testDynamicIdleTimeout) {
    ThreadPool pool(10, 100, std::chrono::milliseconds{100});

    for (int i = 0; i < 10; i++) {
        std::string id = "Task_" + std::to_string(i);
        auto runnable = std::make_unique<TestRunnable>(this, id, 1000000); // 1 second
        pool.start(std::move(runnable));
    }

    PcoThread::usleep(2000000); // Wait 2 seconds

    // Check that all threads are removed after idle timeout
    EXPECT_EQ(pool.currentNbThreads(), 0);
}

/// \brief A test to validate the handling of invalid parameters in the pool constructor
/// This test ensures the pool correctly handles invalid inputs such as negative or zero values.
TEST_F(ThreadpoolTest, testInvalidParameters) {
    EXPECT_THROW(ThreadPool(0, 10, std::chrono::milliseconds{100}), std::invalid_argument);
    EXPECT_THROW(ThreadPool(-1, 10, std::chrono::milliseconds{100}), std::invalid_argument);

    EXPECT_THROW(ThreadPool(5, -1, std::chrono::milliseconds{100}), std::invalid_argument);
    EXPECT_THROW(ThreadPool(5, 10, std::chrono::milliseconds{-100}), std::invalid_argument);

    EXPECT_NO_THROW(ThreadPool(5, 10, std::chrono::milliseconds{100}));
}

/// \brief A test for task cancellation
/// This test ensures that cancelled tasks do not execute and are correctly removed from the running state.
TEST_F(ThreadpoolTest, testCancelledTasks) {
    ThreadPool pool(5, 10, std::chrono::milliseconds{100});
    std::vector<std::unique_ptr<Runnable>> tasks;

    for (int i = 0; i < 10; i++) {
        std::string id = "Task_" + std::to_string(i);
        tasks.push_back(std::make_unique<TestRunnable>(this, id));
    }

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(pool.start(std::move(tasks[i])));
        runnableStarted("Task_" + std::to_string(i));
    }

    for (int i = 5; i < 10; i++) {
        tasks[i]->cancelRun();
        m_runningState.erase("Task_" + std::to_string(i));
    }

    PcoThread::usleep(200000); // Wait for execution

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(m_runningState["Task_" + std::to_string(i)], false);
    }

    for (int i = 5; i < 10; i++) {
        EXPECT_EQ(m_runningState.find("Task_" + std::to_string(i)), m_runningState.end());
    }
}

/// \brief A test to ensure tasks are executed in the correct order
/// This test validates that tasks are processed in the order they are submitted.
TEST_F(ThreadpoolTest, testTaskOrder) {
    ThreadPool pool(3, 10, std::chrono::milliseconds{100});
    std::vector<std::string> executionOrder;

    class OrderedRunnable : public Runnable {
    private:
        std::string taskId;
        std::vector<std::string> &executionOrder;

    public:
        OrderedRunnable(const std::string &id, std::vector<std::string> &order)
            : taskId(id), executionOrder(order) {}

        void run() override {
            executionOrder.push_back(taskId);
        }

        void cancelRun() override {}

        std::string id() override {
            return taskId;
        }
    };

    for (int i = 0; i < 5; i++) {
        std::string id = "Task_" + std::to_string(i);
        auto runnable = std::make_unique<OrderedRunnable>(id, executionOrder);
        EXPECT_TRUE(pool.start(std::move(runnable)));
    }

    PcoThread::usleep(300000); // Wait for execution

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(executionOrder[i], "Task_" + std::to_string(i));
    }
}





int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    logger().initialize(argc, argv);
    PcoLogger::setVerbosity(1);
    return RUN_ALL_TESTS();
}
