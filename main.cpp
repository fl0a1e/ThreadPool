#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include "threadPool.h" 

int main() {
    ThreadPool *p = new ThreadPool(5);  // 创建含有5个线程的线程池
    std::vector<std::future<int>> res;  // 存储异步获取的任务返回值

    for(int i = 0; i < 10; ++i) {
        res.emplace_back(               // 管理任务返回值
            p->enqueue([i]{             // 任务入队
                std::string s = "thread<" + std::to_string(i) + "> start...";
                std::string end = "thread<" + std::to_string(i) + "> end";
                std::cout << s << " " << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << end << " " << std::endl;
                return i*i;
            })
        );
    }
    p->enqueue([]{ // 测试join()，等待几秒后线程池才会退出
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }); 

    for(auto&& r : res) {
        std::cout << r.get() << ' ';
    }
    delete(p);
    return 0;
}