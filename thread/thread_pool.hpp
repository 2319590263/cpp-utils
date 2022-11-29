#ifndef THREADPOOL_HPP_THREAD_POOL_H
#define THREADPOOL_HPP_THREAD_POOL_H

#include <atomic>
#include "thread_safe.hpp"
#include <functional>

using namespace std;


class SimpleThreadPool{

    //盘点线程池是否已完成
    atomic<bool> done = false;
    //任务队列
    thread_safe_queue<function<void()>> work_queue;
    //储存线程的容器
    vector<thread> threads;

    //线程工作方法
    void worker_thread(){
        while(!done){
            function<void()> task;
            if(work_queue.try_pop(task)){
                task();
            }else{
                this_thread::yield();
            }
        }
    }

    void join_all_thread(){
        for_each(threads.begin(),threads.end(),[this](thread &t){
            t.join();
        });
    }

public:
    SimpleThreadPool(){
        unsigned const thread_count = thread::hardware_concurrency();

        try{
            for (unsigned int i = 0; i < thread_count; ++i) {
                threads.emplace_back(&SimpleThreadPool::worker_thread, this);
            }
        }catch (...){
            done = true;
            throw;
        }
    }

    ~SimpleThreadPool(){
        done = true;
        join_all_thread();
    }

    void submit(function<void()> f){
        work_queue.push(f);
    }

};

#endif //THREADPOOL_HPP_THREAD_POOL_H
