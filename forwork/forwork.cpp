#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/python.hpp>
#include <queue>
#include <vector>
#include <iostream>

namespace bp = boost::python;

// PyThread 类，用于管理 GIL 和线程
class PyThread {
public:
    PyThread(const std::string& moduleName, const std::string& functionName, const std::string& event)
        : moduleName(moduleName), functionName(functionName), event(event) {}

    void operator()() {
        // 在每个线程中获得 GIL
        PyGILState_STATE gstate = PyGILState_Ensure();

        try {
            // 导入 Python 模块
            bp::object module = bp::import(moduleName.c_str());
            bp::object func = module.attr(functionName.c_str());
            func(event); // 调用 Python 函数
        } catch (const bp::error_already_set&) {
            PyErr_Print(); // 打印 Python 错误
        }

        // 释放 GIL
        PyGILState_Release(gstate);
    }

private:
    std::string moduleName;
    std::string functionName;
    std::string event;
};

// 线程池类
class ThreadPool {
public:
    ThreadPool(size_t defaultThreads, size_t maxThreads)
        : maxThreads(maxThreads), stop(false) {
        // 启动默认线程
        for (size_t i = 0; i < defaultThreads; ++i) {
			threads.push_back(boost::thread(&ThreadPool::worker, this))
        }
    }

    ~ThreadPool() {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            stop = true; // 设置停止标志
        }
        cv.notify_all(); // 唤醒所有线程
        for (boost::thread &thread : threads) {
            thread.join(); // 等待所有线程完成
        }
    }

    void enqueue(const std::string& event) {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            events.push(event);
        }
        cv.notify_one(); // 通知一个等待的线程

        // 检查是否需要创建新线程
        if (events.size() > threads.size() && threads.size() < maxThreads) {
			threads.push_back(boost::thread(&ThreadPool::worker, this))
        }
    }

private:
    std::vector<boost::thread> threads;
    std::queue<std::string> events; // 存储事件的队列
    boost::mutex mtx;
    boost::condition_variable cv;
    size_t maxThreads;
    bool stop;

    void worker() {
        while (true) {
            std::string event;

            {
                boost::unique_lock<boost::mutex> lock(mtx);
                cv.wait(lock, [this] { return stop || !events.empty(); });

                if (stop && events.empty()) {
                    break; // 如果停止且事件队列为空，则退出线程
                }

                event = events.front();
                events.pop();
            }

            // 创建并运行 PyThread
            PyThread pyThread("your_module", "on_event", event);
            std::thread t(pyThread);
            t.detach(); // 分离线程，使其在后台运行
        }
    }
};

int main() {
    // 初始化 Python 解释器
    Py_Initialize();
    
    // 启动线程池
    ThreadPool pool(2, 5); // 默认线程数为 2，最大线程数为 5

    // 模拟添加事件
    for (int i = 0; i < 10; ++i) {
        pool.enqueue("Event " + std::to_string(i));
    }

    boost::this_thread::sleep_for(boost::chrono::seconds(2)); // 等待任务完成

    // 结束 Python 解释器
    Py_Finalize();

    return 0;
}
