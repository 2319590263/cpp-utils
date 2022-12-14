#include <atomic>
#include "thread_safe.hpp"
#include <functional>
#include <future>
#include<map>
#include <thread>
#include "thread-utils.hpp"
#include <condition_variable>

#pragma once

using namespace std::chrono;

namespace ForestSavage {
    class JAVAThreadPool {
        //任务队列
        thread_safe_queue<function_wrapper> pool_work_queue;
        //核心线程数
        int corePoolSize;
        //最大线程数
        int maximumPoolSize;
        //最大任务数
        int maxNumTask;
        //线程最大空闲时间
        int keepAliveTime;
        //清理线程
        jthread clean_thread;
        //核心线程集合
        vector<jthread> core_threads;
        //最大线程集合
        map<thread::id, jthread> threads;
        //失效线程集合
        vector<jthread> expire_threads;
        //任务提交锁
        mutex m1;
        //线程之间使用的锁
        mutex m2;
//        condition_variable v;

        void execute(function_wrapper &&task) {
            //判断任务队列是否已满,且线程数达到最大
            if ((pool_work_queue.size() >= maxNumTask) && ((core_threads.size() + threads.size()) >= maximumPoolSize)) {
                task();
                return;
            }
            m1.lock();
            //是否新增核心线程
            bool new_core_thread = false;
            //是否要新增线程
            bool new_thread = false;
            //判断核心线程是否创建完成
            if (core_threads.size() < corePoolSize) {
                new_core_thread = true;
            }
                //判断任务队列是否已满,且线程数还没达到最大
            else if ((pool_work_queue.size() >= maxNumTask) &&
                     ((core_threads.size() + threads.size()) < maximumPoolSize)) {
                new_thread = true;
            }

            pool_work_queue.push(move(task));
            if (new_core_thread) {
                jthread tmp(bind(&JAVAThreadPool::worker_core_thread, this, placeholders::_1));
                core_threads.push_back(move(tmp));
            }
            if (new_thread) {
                jthread tmp(bind(&JAVAThreadPool::worker_thread, this, placeholders::_1));
                threads.insert(make_pair(tmp.get_id(), move(tmp)));
            }
            m1.unlock();
        }

        //清理线程工作方法
        void clean_worker_thread(stop_token st) {
            while (!st.stop_requested()) {
                if (!expire_threads.empty()) {
                    m2.lock();
                    expire_threads.clear();
                    m2.unlock();
                } else {
                    this_thread::yield();
                }
            }
        }

        //核心线程工作方法
        void worker_core_thread(stop_token st) {
            while (!st.stop_requested()) {
                function_wrapper task;
                if (pool_work_queue.try_pop(task)) {
                    task();
                } else {
                    this_thread::yield();
                }
            }
        }

        //线程工作方法
        void worker_thread(stop_token st) {
            time_point<system_clock> last_worker_time = system_clock::now();;
            while (!st.stop_requested()) {
                function_wrapper task;
                if (pool_work_queue.try_pop(task)) {
                    last_worker_time = system_clock::now();
                    task();
                } else {
//                    判断线程是否超时
                    if ((last_worker_time + seconds(keepAliveTime)) <= system_clock::now()) {
                        m2.lock();
                        expire_threads.push_back(move(threads[this_thread::get_id()]));
                        m2.unlock();
                        threads.erase(this_thread::get_id());
                        return;
                    }
                    this_thread::yield();
                }
            }
        }

    public:

        JAVAThreadPool() : JAVAThreadPool(thread::hardware_concurrency() / 2,
                                          thread::hardware_concurrency() * 2,
                                          20,
                                          30) {

        }

        JAVAThreadPool(int keepAliveTime) : JAVAThreadPool(thread::hardware_concurrency() / 2,
                                                           thread::hardware_concurrency() * 2,
                                                           20,
                                                           keepAliveTime) {
        }

        JAVAThreadPool(int corePoolSize,
                       int maximumPoolSize,
                       int maxNumTask,
                       int keepAliveTime)
                : corePoolSize(corePoolSize),
                  maximumPoolSize(maximumPoolSize),
                  maxNumTask(maxNumTask),
                  keepAliveTime(keepAliveTime) {
            //创建清理线程
            jthread tmp(bind(&JAVAThreadPool::clean_worker_thread, this, placeholders::_1));
            clean_thread = move(tmp);
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

        int count_thread() {
            return threads.size() + core_threads.size();
        }

        int count_expire_thread() {
            return expire_threads.size();
        }

        void clean_expire_thread() {
            expire_threads.clear();
        }

        ~JAVAThreadPool() {
            wait_pool_exec_finish();
//            v.notify_one();
        }

    };

    namespace Convenient {
        using ForestSavage::JAVAThreadPool;
    }

}
