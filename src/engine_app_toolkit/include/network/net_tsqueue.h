#pragma once

#include <mutex>
#include <thread>
#include <deque>

namespace engine
{
namespace net
{
    // thread safe queue with locks
    template<typename T>
    class ThreadSafeQueue
    {
    private:
        using locker = std::unique_lock<std::mutex>;
    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(const ThreadSafeQueue<T>& rhs) = delete;

        virtual ~ThreadSafeQueue() { clear(); }

    public:
        const T& front()
        {
            locker lock(mtx_queue_);
            return queue_.front();
        }

        const T& back()
        {
            locker lock(mtx_queue_);
            return queue_.back();
        }

        void push_front(const T& item)
        {
            locker lock(mtx_queue_);
            queue_.push_front(item);

            notify_cv_wait_to_wake_up();
        }

        void push_back(const T& item)
        {
            locker lock(mtx_queue_);
            queue_.push_back(item);

            notify_cv_wait_to_wake_up();
        }

        bool empty() const
        {
            locker lock(mtx_queue_);
            return queue_.empty();
        }

        void clear()
        {
            locker lock(mtx_queue_);
            queue_.clear();
        }

        std::size_t count() const
        {
            locker lock(mtx_queue_);
            return queue_.size();
        }

        T pop_front()
        {
            locker lock(mtx_queue_);
            auto t = std::move(queue_.front());
            queue_.pop_front();
            return t;
        }

        T pop_back()
        {
            locker lock(mtx_queue_);
            auto t = std::move(queue_.back());
            queue_.pop_back();
            return t;
        }

        // wait is blocking call - will suspend calling object until something new is pushed to the queue
        void wait()
        {
            // condition variable can wake up due to OS reasons, so we add another guard here: infinity loop to run cv_wait.wait(..) again if there are no items in queue
            while (empty())
            {
                locker lock(mtx_cv_wait_);
                cv_wait.wait(lock);
            }
        }

    private:
        void notify_cv_wait_to_wake_up()
        {
            locker lock_cv_wait(mtx_cv_wait_);
            cv_wait.notify_one();
        }

    private:
        mutable std::mutex mtx_queue_; // mutable to allow to use them in const functions
        std::deque<T> queue_;

        std::condition_variable cv_wait;
        mutable std::mutex mtx_cv_wait_;

    };
}
}