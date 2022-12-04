#ifndef THREADPOOL_HPP_THREAD_POOL_H
#define THREADPOOL_HPP_THREAD_POOL_H

#include <atomic>
#include "thread_safe.hpp"
#include <functional>
#include <future>
#include<map>
#include <thread>

using namespace std;
namespace ForestSavage {
    class ForestSavageThreadPool;

    static ForestSavageThreadPool *pft;

//无视类型调用类
    class function_wrapper {
        struct impl_base {
            virtual void call() = 0;

            virtual ~impl_base() {}
        };

        std::unique_ptr<impl_base> impl;

        template<typename F>
        struct impl_type : impl_base {
            F f;

            impl_type(F &&f_) : f(std::move(f_)) {}

            void call() { f(); }
        };

    public:
        template<typename F>
        function_wrapper(F &&f):
                impl(new impl_type<F>(std::move(f))) {}

        void operator()() { impl->call(); }

        function_wrapper() = default;

        function_wrapper(function_wrapper &&other) :
                impl(std::move(other.impl)) {}

        function_wrapper &operator=(function_wrapper &&other) {
            impl = std::move(other.impl);
            return *this;
        }

        function_wrapper(const function_wrapper &) = delete;

        function_wrapper(function_wrapper &) = delete;

        function_wrapper &operator=(const function_wrapper &) = delete;
    };

//可以偷取任务的线程池的任务队列类
    class work_stealing_queue {
    private:
        typedef function_wrapper data_type;
        deque<data_type> q;
        mutable mutex m;
    public:
        work_stealing_queue() {}

        work_stealing_queue(const work_stealing_queue &other) = delete;

        work_stealing_queue &operator=(const work_stealing_queue &other) = delete;

        void push(data_type data) {
            lock_guard<mutex> lg(m);
            q.push_front(move(data));
        }

        bool empty() const {
            lock_guard<mutex> lg(m);
            return q.empty();
        }

        bool try_pop(data_type &res) {
            lock_guard<mutex> lg(m);
            if (q.empty()) {
                return false;
            }
            res = move(q.front());
            q.pop_front();
            return true;
        }

        bool try_steal(data_type &res) {
            lock_guard<mutex> lg(m);
            if (q.empty()) {
                return false;
            }
            res = move(q.back());
            q.pop_back();
            return true;
        }
    };

//线程本地任务队列
    static thread_local int thread_index;
//线程索引
    static thread_local work_stealing_queue *local_work_queue;

    class ForestSavageThreadPool {

        typedef function_wrapper task_type;
        //全局任务队列
        thread_safe_queue<task_type> pool_work_queue;
        //储存各个线程本地任务队列的指针
        vector<unique_ptr<work_stealing_queue>> queues;
        typedef queue<function_wrapper> local_queue_type;
        //储存线程的容器
        vector<jthread> threads;
        mutex m;
        //线程总数
        const int thread_count;

        //线程工作方法
        void worker_thread(int index_, stop_token st) {
            thread_index = index_;
            local_work_queue = queues[thread_index].get();
            while (!st.stop_requested()) {
                run_pending_task();
            }
        }

        //从本地任务队列获取任务
        bool pop_task_from_local_queue(task_type &task) {
            return local_work_queue && local_work_queue->try_pop(task);
        }

        //从全局任务队列获取任务
        bool pop_task_from_pool_queue(task_type &task) {
            return pool_work_queue.try_pop(task);
        }

        //从其他线程任务队列窃取任务
        bool pop_task_from_other_thread_queue(task_type &task) {
            for (int i = 0; i < queues.size(); ++i) {
                const int index = (thread_index + i + 1) % queues.size();
                if (queues[index]->try_steal(task)) {
                    return true;
                }
            }
            return false;
        }

        ForestSavageThreadPool() : thread_count(thread::hardware_concurrency()) {
            try {
                for (int i = 0; i < thread_count; ++i) {
                    queues.push_back(unique_ptr<work_stealing_queue>(new work_stealing_queue));
                    auto worker_fun = mem_fn(&ForestSavageThreadPool::worker_thread);
                    auto fun = bind(worker_fun, this, i, placeholders::_1);
                    threads.emplace_back(fun);
                }
            } catch (...) {
                throw;
            }
        }

        ~ForestSavageThreadPool() {
            wait_pool_exec_finish();
        }

    public:


        template<typename FunctionType>
        future<typename result_of<FunctionType()>::type>
        submit(FunctionType f) {
            typedef typename result_of<FunctionType()>::type
                    result_type;
            packaged_task<result_type()> task(f);
            future<result_type> res(task.get_future());
            task_type fw = task_type(move(task));
            //判断是否是池内线程，若是池内线程则直接往本地任务队列添加任务，若不是则往全局任务队列添加
            if (local_work_queue) {
                local_work_queue->push(move(fw));
            } else {
                pool_work_queue.push(move(fw));
            }
            return res;
        }

        void run_pending_task() {
            task_type task;
            //以此从任务队列中获取任务，若没有任务则放弃cpu时间片
            if (pop_task_from_local_queue(task) ||
                pop_task_from_pool_queue(task) ||
                pop_task_from_other_thread_queue(task)) {
                task();
            } else {
                this_thread::yield();
            }
        }

        //判断线程池是否可关闭，判断标准为全局任务队列以及各线程的本地任务队列是否都为空
        bool empty() {
            bool result = pool_work_queue.empty();
            for (int i = 0; i < thread_count; ++i) {
                result &= queues[i]->empty();
            }
            return result;
        }

        void wait_pool_exec_finish(){
            while (!empty()){
                this_thread::yield();
            }
        }

        friend ForestSavageThreadPool *get_pool();

        friend void close_pool();

    };


    ForestSavageThreadPool *get_pool() {
        if (!pft) {
            pft = new ForestSavageThreadPool();
        }
        return pft;
    }

    void close_pool() {
        if (pft) {
            delete pft;
            pft = nullptr;
        }
    }

}

namespace ForestSavageQuick{
    using ForestSavage::get_pool;
    using ForestSavage::close_pool;
    using ForestSavage::ForestSavageThreadPool;
}
#endif //THREADPOOL_HPP_THREAD_POOL_H
