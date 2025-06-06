#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>

namespace utils {

/**
 * @brief 通用线程池类
 * 提供高效的任务调度和执行功能
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param numThreads 线程池中的线程数量，默认为系统硬件并发数
     */
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) : stop_(false) {
        // 确保至少有一个线程
        numThreads = numThreads > 0 ? numThreads : 1;
        
        for(size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while(true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if(stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    /**
     * @brief 析构函数，停止所有线程
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        for(std::thread &worker : workers_) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }
    
    /**
     * @brief 提交任务到线程池并获取future结果
     * @param f 要执行的函数
     * @param args 函数参数
     * @return std::future对象，用于获取任务执行结果
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if(stop_) {
                throw std::runtime_error("任务提交到已停止的线程池");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    /**
     * @brief 简化版任务提交方法，不返回结果（与旧接口兼容）
     * @param task 任务函数
     */
    template<class F>
    void submit(F&& task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            if(stop_) {
                return;
            }
            
            tasks_.emplace(std::forward<F>(task));
        }
        
        condition_.notify_one();
    }
    
    /**
     * @brief 获取线程池中的线程数量
     * @return 线程数量
     */
    size_t size() const {
        return workers_.size();
    }
    
    /**
     * @brief 获取当前队列中等待的任务数量
     * @return 任务数量
     */
    size_t queueSize() const {
        std::unique_lock<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }
    
    /**
     * @brief 等待所有任务完成
     */
    void waitAll() {
        std::unique_lock<std::mutex> lock(queueMutex_);
        condition_.wait(lock, [this] { 
            return tasks_.empty(); 
        });
    }

private:
    std::vector<std::thread> workers_;                  // 工作线程
    std::queue<std::function<void()>> tasks_;           // 任务队列
    
    mutable std::mutex queueMutex_;                     // 队列互斥锁
    std::condition_variable condition_;                 // 条件变量
    std::atomic<bool> stop_;                            // 停止标志
};

} // namespace utils 