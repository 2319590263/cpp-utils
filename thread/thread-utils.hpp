#include <thread>
#include <iostream>
#include <deque>

#pragma once

using namespace std;

namespace ForestSavage {

    class function_wrapper {
        struct impl_base {
            virtual void call() = 0;

            virtual ~impl_base() {}
        };

        unique_ptr<impl_base> impl;

        template<typename F>
        struct impl_type : impl_base {
            F f;

            impl_type(F &&f_) : f(move(f_)) {}

            void call() { f(); }
        };

    public:
        template<typename F>
        function_wrapper(F &&f):
                impl(new impl_type<F>(move(f))) {}

        void operator()() { impl->call(); }

        function_wrapper() = default;

        function_wrapper(function_wrapper &&other) :
                impl(move(other.impl)) {}

        function_wrapper &operator=(function_wrapper &&other) {
            impl = move(other.impl);
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

    namespace Convenient {}
}