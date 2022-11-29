//
// Created by yeren on 2022-11-29.
//

#ifndef THREADPOOL_HPP_THREAD_SAFE_HPP
#define THREADPOOL_HPP_THREAD_SAFE_HPP

#include <memory> // 为了使用std::shared_ptr
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

template<typename T>
class thread_safe_queue {
    mutable mutex m;
    condition_variable c;
    queue<T> q;
public:
    thread_safe_queue(){}

    thread_safe_queue(const thread_safe_queue& tq){
        lock_guard<mutex> lg(m);
        q = tq.q;
    }

    thread_safe_queue &operator=(const thread_safe_queue &) = delete; // 不允许简单的赋值

    void push(T new_value){
        lock_guard<mutex> lg(m);
        q.push(new_value);
        c.notify_one();
    }

    bool try_pop(T &value){
        lock_guard<mutex> lg(m);
        if(!q.empty()){
            value = q.front();
            q.pop();
            return true;
        }
        return false;
    }
    shared_ptr<T> try_pop(){
        lock_guard<mutex> lg(m);
        shared_ptr<T> sp = nullptr;
        if(!q.empty()){
            sp = make_shared<T>(q.front());
            q.pop();
        }
        return sp;
    }
    void wait_and_pop(T &value){
        unique_lock<mutex> ul(m);
        c.wait(ul,[this]{return !q.empty();});
        value = q.front();
        q.pop();
    }

    shared_ptr<T> wait_and_pop(){
        unique_lock<mutex> ul(m);
        c.wait(ul,[this]{return !q.empty();});
        shared_ptr<T> sp = make_shared<T>(q.front());
        q.pop();
        return sp;
    }

    bool empty() const{
        lock_guard<mutex> lg(m);
        return q.empty();
    }

    int size() const{
        return q.size();
    }
};

#endif //THREADPOOL_HPP_THREAD_SAFE_HPP
