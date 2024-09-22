// testlua.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <lua.hpp>

#include <iostream>
using namespace std;

#include "luabind\luabind.hpp"


struct TEvent {
	string name;
};

// 绑定结构体到 Lua
void bindEvent(lua_State* L) {
    luabind::module(L)[
        luabind::class_<TEvent>("TEvent")
            .def_readwrite("name", &TEvent::name)
    ];
}

class TLuaThread {
public:
    TLuaThread(const std::string& script): script(script), need_load(false) {
        // 初始化 Lua 解释器
        L = luaL_newstate();
        luaL_openlibs(L);

			luabind::open(L);

	 bindEvent(L); // 绑定 Point 结构体
        
        // 执行初始化脚本
        load_file();
    }

    ~TLuaThread() {
        lua_close(L); // 关闭 Lua 解释器
    }

	void load_file() {
		if (luaL_dofile(L, script.c_str()) != 0) {
            std::cerr << "Failed to load script: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
	}

	void print_lua_error(lua_State *L) {
    // 获取错误信息
    if (lua_gettop(L) > 0) {
        const char *error_msg = lua_tostring(L, -1);
        if (error_msg) {
            std::cerr << "Lua error: " << error_msg << std::endl;
        }
        // 移除错误信息，保持栈的整洁
        lua_pop(L, 1);
    }
}

    void onEvent(const TEvent& event) {
		if (need_load) {
			load_file();
			need_load = false;
		}
		try {
        // 调用 Lua 的 greet 函数
			TEvent event;
			luabind::call_function<void>(L, "on_event", &event);
			std::cout << event.name << std::endl; // 输出: Hello, World!
		} catch (const luabind::error& e) {
			print_lua_error(L);
		}
    }

private:
    lua_State* L; // Lua 状态
	std::string script;
public:
	bool need_load;
};


#include <vector>
#include <queue>
#include <boost/thread.hpp>

class TLuaThreadPool {
public:
    TLuaThreadPool(size_t minThreads, size_t maxThreads, const std::string& initScript)
        : minThreads(minThreads), maxThreads(maxThreads), initScript(initScript), stop(false) {

        // 启动最小线程
        for (size_t i = 0; i < minThreads; ++i) {
			TLuaThread* lua_thread = new TLuaThread(initScript);
			lua_threads.push_back(lua_thread);
            threads.push_back(boost::thread(&TLuaThreadPool::worker, this, lua_thread));
        }
    }

void close() {
	 {
            boost::unique_lock<boost::mutex> lock(mtx);
            stop = true; // 设置停止标志
        }
        cv.notify_all(); // 唤醒所有线程

        // 使用传统的 for 循环等待所有线程完成
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join(); // 等待每个线程完成
        }
}

~TLuaThreadPool() {

    }

    void on_event(const TEvent& event) {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            events.push(event);
        }
        cv.notify_one(); // 通知一个等待的线程

        // 动态扩容逻辑
        if (events.size() > threads.size() && threads.size() < maxThreads) {
           TLuaThread* lua_thread = new TLuaThread(initScript);
			lua_threads.push_back(lua_thread);
            threads.push_back(boost::thread(&TLuaThreadPool::worker, this, lua_thread));

			std::cout << threads.size() << std::endl;
        }
    }

	void reload_all() {
		for (size_t i = 0; i < lua_threads.size(); ++i) {
            lua_threads[i]->need_load = true; // 等待每个线程完成
        }
	}

private:
	std::vector<TLuaThread*> lua_threads;
    std::vector<boost::thread> threads;
    std::queue<TEvent> events; // 存储事件的队列
    boost::mutex mtx;
    boost::condition_variable cv;
    size_t minThreads;
    size_t maxThreads;
    bool stop;
	std::string initScript;

    void worker(TLuaThread* lua_thread) {
        while (true) {
            TEvent event;

            {
                boost::unique_lock<boost::mutex> lock(mtx);
                cv.wait(lock, [this] { return stop || !events.empty(); });

                if (stop) {
                    break; // 如果停止且事件队列为空，则退出线程
                }

				if (events.empty()) {
					continue;
				}

                event = events.front();
                events.pop();
            }

            lua_thread->onEvent(event); // 处理事件
        }
    }
};



int _tmain(int argc, _TCHAR* argv[])
{
	    // 创建一个 Lua 状态机
  TLuaThreadPool thread_pool(2, 2, "root.lua");

  TEvent event;

  for (int i=0; i<100; ++i) {
	thread_pool.on_event(event);
  }

  getchar();

  thread_pool.reload_all();

	  for (int i=0; i<100; ++i) {
	thread_pool.on_event(event);
  }

	  getchar();


	return 0;
}

