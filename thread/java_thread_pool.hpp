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
        int maxNumTask;
        int keepAliveTime;
        map<thread::id, jthread> threads;
        vector<jthread> expire_threads;
        mutex m;

        void execute(function_wrapper &&task) {
            unique_lock<mutex> ul(m);
            //是否要新增线程
            bool new_thread_flag = false;
            //是否要提交任务
            bool push_task_flag = false;
            //判断核心线程是否创建完成
            if (threads.size() < corePoolSize) {
                new_thread_flag = true;
                push_task_flag = true;
            }
                //判断任务队列是否空余以及核心线程是否已满
            else if ((pool_work_queue.size() < maxNumTask) && (threads.size() >= corePoolSize)) {
                push_task_flag = true;
            }
                //判断任务队列是否已满,且线程数还没达到最大
            else if ((pool_work_queue.size() >= maxNumTask) && (threads.size() < maximumPoolSize)) {
                new_thread_flag = true;
                push_task_flag = true;
            }
                //判断任务队列是否已满,且线程数达到最大
            else if ((pool_work_queue.size() >= maxNumTask) && (threads.size() >= maximumPoolSize)) {}
            else { throw; }

            if (push_task_flag) {
                pool_work_queue.push(move(task));
            }
            if (new_thread_flag) {
                jthread tmp(bind(&JAVAThreadPool::worker_thread, this, placeholders::_1));
                threads.insert(make_pair(tmp.get_id(), move(tmp)));
            }
            ul.unlock();
            if (!new_thread_flag && !push_task_flag) {
                task();
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
                    //如果有等待被清理的线程则进行清理
                    if (!expire_threads.empty()) {
                        m.lock();
                        expire_threads.clear();
                        m.unlock();
                    }
//                    判断线程是否超时以及当前线程数是否小等于核心线程数
                    if ((last_worker_time + seconds(keepAliveTime)) <= system_clock::now()) {
                        m.lock();
                        expire_threads.push_back(move(threads[this_thread::get_id()]));
                        m.unlock();
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
                                          3) {

        }

        JAVAThreadPool(int corePoolSize,
                       int maximumPoolSize,
                       int maxNumTask,
                       int keepAliveTime)
                : corePoolSize(corePoolSize),
                  maximumPoolSize(maximumPoolSize),
                  maxNumTask(maxNumTask),
                  keepAliveTime(keepAliveTime) {

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
            return threads.size();
        }

        int count_expire_thread() {
//            for (auto &t:expire_threads) {
//                cout<<t.get_id()<<endl;
//            }
            return expire_threads.size();
        }

        void clean_expire_thread(){
            expire_threads.clear();
        }

        ~JAVAThreadPool() {
            wait_pool_exec_finish();
        }

    };

    namespace Convenient {
        using ForestSavage::JAVAThreadPool;
    }

}
