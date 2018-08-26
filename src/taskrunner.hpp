#ifndef TASKRUNNER_HPP_
#define TASKRUNNER_HPP_

#include "threadpool.hpp"

/** Runs tasks in parallel using a thread pool. */
template <typename Result>
class TaskRunner {
public:
    /** Constructor. The thread pool must stay alive as long as this object does.
     * The thread pool must not be used for something else while being used by this object. */
    TaskRunner(ThreadPool<int>& pool);
    /** Destructor. */
    ~TaskRunner();

    /** Add a task to be executed. The function "func" must have signature:
     *  Result func(int workerNo), where workerNo is between 0 and nThreads-1.
     *  Task execution starts in the same order as the tasks were added. */
    template <typename Func>
    void addTask(Func func);

    /** Wait for and retrieve a result. Return false if there is no task to wait for.
     *  The results are not necessarily returned in the same order as the tasks were added. */
    bool getResult(Result& result);

    /** Wait for all added tasks and throw away their results. */
    void waitAll();

private:
    ThreadPool<int>& pool;
    std::mutex resultsMutex;
    std::deque<Result> results;
    std::mutex getResultMutex;
};

template <typename Result>
TaskRunner<Result>::TaskRunner(ThreadPool<int>& pool)
    : pool(pool) {
}

template <typename Result>
TaskRunner<Result>::~TaskRunner() {
}

template <typename Result>
template <typename Func>
void TaskRunner<Result>::addTask(Func func) {
    auto task = [this,func](int workerNo) {
        Result result = func(workerNo);
        std::unique_lock<std::mutex> L(resultsMutex);
        results.push_back(std::move(result));
        return 0;
    };
    pool.addTask(task);
}

template <typename Result>
bool TaskRunner<Result>::getResult(Result& result) {
    std::unique_lock<std::mutex> L(getResultMutex);
    int dummy;
    if (pool.getResult(dummy)) {
        std::unique_lock<std::mutex> L2(resultsMutex);
        result = std::move(results.front());
        results.pop_front();
        return true;
    }
    return false;
}

template <typename Result>
void TaskRunner<Result>::waitAll() {
    Result result;
    while (getResult(result))
        ;
}

#endif
