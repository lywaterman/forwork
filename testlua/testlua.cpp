// testlua.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#include <lua.hpp>

#include <iostream>
using namespace std;

#include "luabind\luabind.hpp"


struct TEvent {
	string name;
};

// �󶨽ṹ�嵽 Lua
void bindEvent(lua_State* L) {
    luabind::module(L)[
        luabind::class_<TEvent>("TEvent")
            .def_readwrite("name", &TEvent::name)
    ];
}

class TLuaThread {
public:
    TLuaThread(const std::string& script): script(script), need_load(false) {
        // ��ʼ�� Lua ������
        L = luaL_newstate();
        luaL_openlibs(L);

			luabind::open(L);

	 bindEvent(L); // �� Point �ṹ��
        
        // ִ�г�ʼ���ű�
        load_file();
    }

    ~TLuaThread() {
        lua_close(L); // �ر� Lua ������
    }

	void load_file() {
		if (luaL_dofile(L, script.c_str()) != 0) {
            std::cerr << "Failed to load script: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
	}

	void print_lua_error(lua_State *L) {
    // ��ȡ������Ϣ
    if (lua_gettop(L) > 0) {
        const char *error_msg = lua_tostring(L, -1);
        if (error_msg) {
            std::cerr << "Lua error: " << error_msg << std::endl;
        }
        // �Ƴ�������Ϣ������ջ������
        lua_pop(L, 1);
    }
}

    void onEvent(const TEvent& event) {
		if (need_load) {
			load_file();
			need_load = false;
		}
		try {
        // ���� Lua �� greet ����
			TEvent event;
			luabind::call_function<void>(L, "on_event", &event);
			std::cout << event.name << std::endl; // ���: Hello, World!
		} catch (const luabind::error& e) {
			print_lua_error(L);
		}
    }

private:
    lua_State* L; // Lua ״̬
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

        // ������С�߳�
        for (size_t i = 0; i < minThreads; ++i) {
			TLuaThread* lua_thread = new TLuaThread(initScript);
			lua_threads.push_back(lua_thread);
            threads.push_back(boost::thread(&TLuaThreadPool::worker, this, lua_thread));
        }
    }

void close() {
	 {
            boost::unique_lock<boost::mutex> lock(mtx);
            stop = true; // ����ֹͣ��־
        }
        cv.notify_all(); // ���������߳�

        // ʹ�ô�ͳ�� for ѭ���ȴ������߳����
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join(); // �ȴ�ÿ���߳����
        }
}

~TLuaThreadPool() {

    }

    void on_event(const TEvent& event) {
        {
            boost::unique_lock<boost::mutex> lock(mtx);
            events.push(event);
        }
        cv.notify_one(); // ֪ͨһ���ȴ����߳�

        // ��̬�����߼�
        if (events.size() > threads.size() && threads.size() < maxThreads) {
           TLuaThread* lua_thread = new TLuaThread(initScript);
			lua_threads.push_back(lua_thread);
            threads.push_back(boost::thread(&TLuaThreadPool::worker, this, lua_thread));

			std::cout << threads.size() << std::endl;
        }
    }

	void reload_all() {
		for (size_t i = 0; i < lua_threads.size(); ++i) {
            lua_threads[i]->need_load = true; // �ȴ�ÿ���߳����
        }
	}

private:
	std::vector<TLuaThread*> lua_threads;
    std::vector<boost::thread> threads;
    std::queue<TEvent> events; // �洢�¼��Ķ���
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
                    break; // ���ֹͣ���¼�����Ϊ�գ����˳��߳�
                }

				if (events.empty()) {
					continue;
				}

                event = events.front();
                events.pop();
            }

            lua_thread->onEvent(event); // �����¼�
        }
    }
};



int _tmain(int argc, _TCHAR* argv[])
{
	    // ����һ�� Lua ״̬��
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

