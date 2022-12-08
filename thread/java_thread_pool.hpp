#include <atomic>
#include "thread_safe.hpp"
#include <functional>
#include <future>
#include<map>
#include <thread>
#include "thread-utils.hpp"

#pragma once

using namespace std::chrono;

namespace ForestSavage {
    class JAVAThreadPool {
        //任务队列
        thread_safe_queue<function_wrapper> pool_work_queue;
        int corePoolSize;
        int maximumPoolSize;
        seconds keepAliveTime;
        vector<jthread> threads;

        //线程工作方法
        void worker_thread(int index, stop_token st) {
            while (!st.stop_requested()) {
                function_wrapper task;
                if (pool_work_queue.try_pop(task)) {
                    task();
                } else {

                    this_thread::yield();
                }
            }
        }

        void execute(function_wrapper &&fw) {
            if (threads.size() < corePoolSize) {
                threads.emplace_back(bind(&JAVAThreadPool::worker_thread, this, threads.size(), placeholders::_1));
            }
            pool_work_queue.push(move(fw));
        }

    public:

        JAVAThreadPool() : JAVAThreadPool(thread::hardware_concurrency() / 2,
                                          thread::hardware_concurrency() * 2,
                                          60) {

        }

        JAVAThreadPool(int keepAliveTime) : keepAliveTime(keepAliveTime) {

        }

        JAVAThreadPool(int corePoolSize,
                       int maximumPoolSize,
                       int keepAliveTime) :
                corePoolSize(corePoolSize),
                maximumPoolSize(maximumPoolSize),
                keepAliveTime(seconds(keepAliveTime)) {

        }

        template<typename FunctionType>
        future<typename result_of<FunctionType()>::type>
        submit(FunctionType f) {
            typedef typename result_of<FunctionType()>::type
                    result_type;
            packaged_task<result_type()> task(f);
            future<result_type> res(task.get_future());
            execute(function_wrapper(move(task)));
            return res;
        }

        void wait_pool_exec_finish() {
            while (!pool_work_queue.empty()) {
                this_thread::yield();
            }
        }

        ~JAVAThreadPool() {
            wait_pool_exec_finish();
        }

    };

    namespace Convenient {
        using ForestSavage::JAVAThreadPool;
    }

}
