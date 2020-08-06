#include "CancelableThreadPool.h"
#include "utils/Log.h"
#include "utils/ThreadUtils.h"

namespace carto {

    CancelableThreadPool::CancelableThreadPool() :
        _poolSize(0),
        _taskCount(0),
        _stop(false),
        _taskRecords(),
        _workers(),
        _threads(),
        _condition(),
        _mutex()
    {
    }
    
    CancelableThreadPool::~CancelableThreadPool() {
    }
        
    void CancelableThreadPool::deinit() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        
        cancelAll();
        
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _condition.notify_all();
        }
        
        for (std::thread& thread : _threads) {
            thread.detach();
        }
        
        _workers.clear();
        _threads.clear();
    }
    
    int CancelableThreadPool::getPoolSize() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _poolSize;
    }
    
    void CancelableThreadPool::setPoolSize(int poolSize) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_stop) {
            return;
        }
    
        // Add threads
        for (int i = _poolSize; i < poolSize; i++) {
            _workers.push_back(std::make_shared<TaskWorker>(shared_from_this()));
            _threads.push_back(std::thread(&TaskWorker::operator(), _workers.back()));
        }
    
        _poolSize = poolSize;
    }
    
    void CancelableThreadPool::execute(std::shared_ptr<CancelableTask> task) {
        execute(task, DEFAULT_PRIORITY);
    }
    
    void CancelableThreadPool::execute(std::shared_ptr<CancelableTask> task, int priority) {
        if (!task->isCanceled()) {
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_stop) {
                return;
            }
    
            // Push task to queue, increase global task count
            _taskRecords.push(TaskRecord(task, priority, _taskCount));
            _taskCount++;

            // Check if all executors are busy executing lower priority tasks.
            // In that case add a new worker thread to process the current task.
            bool createWorker = true;
            for (const std::shared_ptr<TaskWorker>& worker : _workers) {
                if (!worker->_busy || worker->_currentTaskPriority >= priority) {
                    createWorker = false;
                    break;
                }
            }
            if (createWorker) {
                Log::Debugf("CancelableThreadPool: Adding worker %d to the pool", (int)_workers.size());
                _workers.push_back(std::make_shared<TaskWorker>(shared_from_this()));
                _threads.push_back(std::thread(&TaskWorker::operator(), _workers.back()));
            }
    
            // If there are any waiting threads, notify one of them
            _condition.notify_one();
        }
    }
    
    void CancelableThreadPool::cancelAll() {
        std::lock_guard<std::mutex> lock(_mutex);
        
        std::size_t taskRecordsSize = _taskRecords.size();
        for (std::size_t i = 0; i < taskRecordsSize; i++) {
            const std::shared_ptr<CancelableTask>& task = _taskRecords.top()._task;
            task->cancel();
            _taskRecords.pop();
        }
    }
    
    CancelableThreadPool::TaskRecord::TaskRecord(std::shared_ptr<CancelableTask> task, int priority, long long sequence) :
        _task(task),
        _priority(priority),
        _sequence(sequence)
    {
    }
    
    bool CancelableThreadPool::TaskRecord::operator <(const TaskRecord& taskRecord) const {
        // Tasks are sorted according to their priority and then their sequence
        if (_priority != taskRecord._priority) {
            return _priority < taskRecord._priority;
        }
        return _sequence > taskRecord._sequence;
    }
    
    CancelableThreadPool::TaskWorker::TaskWorker(const std::shared_ptr<CancelableThreadPool>& threadPool) :
        _busy(false),
        _currentTaskPriority(0),
        _threadPool(threadPool)
    {
    }
        
    void CancelableThreadPool::TaskWorker::operator ()() {
        ThreadUtils::SetThreadPriority(ThreadPriority::MINIMUM);
        while (true) {
            auto threadPool = _threadPool.lock();
            if (!threadPool) {
                return;
            }

            // If there are no tasks, wait until notified or exit thread if interrupted
            {
                std::unique_lock<std::mutex> lock(threadPool->_mutex);
                if (threadPool->_stop) {
                    return;
                }
                
                if (threadPool->_taskRecords.size() == 0) {
                    threadPool->_condition.wait(lock);
                }
            }
    
            // Request another task, execute it if it's not null
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(threadPool->_mutex);
                    if (threadPool->_stop) {
                        return;
                    }
                }
                
                std::shared_ptr<CancelableTask> task;
                int priority = DEFAULT_PRIORITY;
                if (threadPool->getNextTask(task, priority)) {
                    {
                        std::lock_guard<std::mutex> lock(threadPool->_mutex);
                        _busy = true;
                        _currentTaskPriority = priority;
                    }

                    task->operator ()();

                    {
                        std::lock_guard<std::mutex> lock(threadPool->_mutex);
                        _busy = false;
                        _currentTaskPriority = DEFAULT_PRIORITY;
                    }
                } else {
                    if (threadPool->shouldTerminateWorker(*this)) {
                        return;
                    }
                    break;
                }

                // Check for interruption
                std::this_thread::yield();
            }
        }
    }
    
    bool CancelableThreadPool::getNextTask(std::shared_ptr<CancelableTask>& task, int& priority) {
        std::lock_guard<std::mutex> lock(_mutex);
    
        // Return the next highest priority task from the task queue
        if (_taskRecords.size() > 0) {
            task = _taskRecords.top()._task;
            priority = _taskRecords.top()._priority;
            _taskRecords.pop();
            return true;
        }
        return false;
    }
    
    bool CancelableThreadPool::shouldTerminateWorker(TaskWorker& worker) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_stop) {
            return true;
        }

        // If there are too many threads, remove this worker and it's thread
        if (static_cast<int>(_threads.size()) > _poolSize) {
            // Find the index of the finished worker, it's thread will have the same index in _threads vector
            for (std::size_t index = 0; index < _workers.size(); index++) {
                if (_workers[index].get() == &worker) {
                    // Remove thread and worker
                    Log::Debugf("CancelableThreadPool: Removing worker %d from the pool", (int)index);
                    _workers.erase(_workers.begin() + index);
                    _threads.at(index).detach();
                    _threads.erase(_threads.begin() + index);
                    break;
                }
            }
            return true;
        }
        return false;
    }

    const int CancelableThreadPool::DEFAULT_PRIORITY = 0;
    
}
