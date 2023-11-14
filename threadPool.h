#ifndef THREAD_POOL_H_  // 防止重复引用头文件
#define THREAD_POOL_H_  // 宏定义

#include <queue>                // 队列，std::queue
#include <mutex>                // 信号量，std::mutex
#include <vector>               // 可变数组，std::vector
#include <thread>               // 线程，std::thread
#include <memory>               // 智能指针，std::make_shared
#include <future>               // 异步处理，std::future，std::packaged_task
#include <utility>              // std::move
#include <iostream>             // I/O，std::cout
#include <stdexcept>            // 异常处理，std::runtime_error
#include <functional>           // 函数管理，std::function
#include <condition_variable>   // 条件变量，std::condition_variable


class ThreadPool {
public:
    ThreadPool(size_t); // 构造函数，初始化几个线程，size_t为了更好的跨平台

    template <class F, class... Args>                               // 模板函数
    auto enqueue(F&& f, Args&&... args)                             // 参数为函数和其参数列表
        -> std::future<typename std::result_of<F(Args...)>::type>;  // 箭头表示返回类型

    ~ThreadPool();      // 析构函数，管理池中所有线程的退出
private:
    // 资源相关
    std::vector<std::thread> workers;        // 线程池
    std::queue<std::function<void()>> tasks; // 任务队列

    // 同步互斥相关
    bool stop;                  // 标识线程池是否关闭，临界资源
    std::mutex queue_mutex;     // 任务队列，上锁保证线程互斥访问，临界资源
    std::condition_variable cv; // 线程同步，用于阻塞、唤醒线程
};

ThreadPool::ThreadPool(size_t thread_nums)
    : stop(false) {       // 启动线程池
    for(size_t i = 0; i < thread_nums; ++i) {
        workers.emplace_back([this]{
            while(true) { // 线程无线循环，但是没有任务时wait()阻塞，不占用cpu

                std::function<void()> task; // 类似回调，接收一个任务队列中的函数

                // 临界区
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);// 获取锁
                    this->cv.wait(lock, [this]{                          // 线程等待，阻塞
                        return this->stop || !this->tasks.empty();       // 线程池关闭或者队列还有任务，进行下一步 
                    });
                    if(this->stop && this->tasks.empty())                // 线程池关闭并且无任务，线程退出
                        return ;
                    task = std::move(this->tasks.front());               // 取队头任务，move避免拷贝性能更好
                    this->tasks.pop();                                   // 删除队头任务
                } // 退出作用域，自动解锁

                task(); // 执行任务
            }
        });
    }
}

// 任务入队
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {

    using return_type = typename std::result_of<F(Args...)>::type;

    // std::forward，完美转发，转发原有参数的左右值属性，不会有不必要的拷贝，增强性能
    // ...用于展开参数
    // bind绑定函数和参数，返回一个可调用对象
    // std::packaged_task，实现异步获取结果
    // std::make_shared，创建shared_ptr智能指针
    // 当f的返回值是int时，task的类型为std::shared_ptr<std::packaged_task<int()>>
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    // 返回值是int时，std::future<int>，异步获取结果，替代全局变量，线程安全
    // std::packaged_task::get_future()
    std::future<return_type> res = task->get_future();

    // 临界区(操作队列)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);               // 获取锁
        if(stop)                                                      // 线程池不可用时，不能再插入任务
            throw std::runtime_error("enqueue on stopped ThreadPool");// 抛出异常
        auto callback = [task]{(*task)();};                           // 定义用于执行task的函数 
        tasks.emplace(callback);                                      // 入队
    } // 退出作用域，自动解锁

    cv.notify_one(); // 随机唤醒一个线程

    return res;
}

ThreadPool::~ThreadPool() {
    std::cout << std::endl << "threadPool closing..." << std::endl;
    {
        std::unique_lock<std::mutex> lock(queue_mutex); // 获取锁
        stop = true;    // 关闭线程池
    }                   // 退出临界区，自动解锁

    cv.notify_all();    // 唤醒所有线程 -> 根据线程代码，线程解除阻塞，进入if判断然后退出

    for(std::thread& worker : workers) { // 遍历
        worker.join();  // join，线程池等待所有未完成线程结束后再退出
    }
    std::cout << "threadPool stop." << std::endl;
}

#endif // THREAD_POOL_H_