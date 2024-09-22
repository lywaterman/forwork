#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/python.hpp>
#include <queue>
#include <vector>
#include <iostream>

namespace bp = boost::python;

// PyThread �࣬���ڹ��� GIL ���߳�
class PyThread {
public:
    PyThread(const std::string& moduleName, const std::string& functionName, const std::string& event)
        : moduleName(moduleName), functionName(functionName), event(event) {}

    void operator()() {
        // ��ÿ���߳��л�� GIL
        PyGILState_STATE gstate = PyGILState_Ensure();

        try {
            // ���� Python ģ��
            bp::object module = bp::import(moduleName.c_str());
            bp::object func = module.attr(functionName.c_str());
            func(event); // ���� Python ����
        } catch (const bp::error_already_set&) {
            PyErr_Print(); // ��ӡ Python ����
        }

        // �ͷ� GIL
        PyGILState_Release(gstate);
    }

private:
    std::string moduleName;
    std::string functionName;
    std::string event;
};

// �̳߳���
class ThreadPool {
public:
    ThreadPool(size_t defaultThreads, size_t maxThreads)
        : maxThreads(maxThreads), stop(false) {
        // ����Ĭ���߳�
        for (size_t i = 0; i < defaultThreads; ++i) {
			threads.push_back(boost::thread(&ThreadPool::worker, this))
        }
    }

    ~ThreadPool() {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            stop = true; // ����ֹͣ��־
        }
        cv.notify_all(); // ���������߳�
        for (boost::thread &thread : threads) {
            thread.join(); // �ȴ������߳����
        }
    }

    void enqueue(const std::string& event) {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            events.push(event);
        }
        cv.notify_one(); // ֪ͨһ���ȴ����߳�

        // ����Ƿ���Ҫ�������߳�
        if (events.size() > threads.size() && threads.size() < maxThreads) {
			threads.push_back(boost::thread(&ThreadPool::worker, this))
        }
    }

private:
    std::vector<boost::thread> threads;
    std::queue<std::string> events; // �洢�¼��Ķ���
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
                    break; // ���ֹͣ���¼�����Ϊ�գ����˳��߳�
                }

                event = events.front();
                events.pop();
            }

            // ���������� PyThread
            PyThread pyThread("your_module", "on_event", event);
            std::thread t(pyThread);
            t.detach(); // �����̣߳�ʹ���ں�̨����
        }
    }
};

int main() {
    // ��ʼ�� Python ������
    Py_Initialize();
    
    // �����̳߳�
    ThreadPool pool(2, 5); // Ĭ���߳���Ϊ 2������߳���Ϊ 5

    // ģ������¼�
    for (int i = 0; i < 10; ++i) {
        pool.enqueue("Event " + std::to_string(i));
    }

    boost::this_thread::sleep_for(boost::chrono::seconds(2)); // �ȴ��������

    // ���� Python ������
    Py_Finalize();

    return 0;
}
