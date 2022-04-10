#include <atomic>
#include <thread>
#include <iostream>
#include <list>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <stdexcept>
template <typename T>
class ThreadSafeQueue
{
private:
    enum class State
    {
        OPEN,
        CLOSED

    };
    State state;
    size_t currentSize;
    size_t maxSize;
    std::condition_variable Push,Pop;
    std::mutex mutex;
    std::list<T> list;
public:
    enum QueueResult
    {
        ENTER,CLOSED
    };
    explicit ThreadSafeQueue(size_t maxSize = 0):state(State::OPEN),currentSize(0),maxSize(maxSize){}
    void push(T const& data) {
        decltype(list) type;
        type.push_back(data);
        {
            std::unique_lock<std::mutex> lock(mutex);
            while (currentSize == maxSize)
                Push.wait(lock);
            if (state == State::CLOSED)
                throw std::runtime_error("The queue is closed,can't trying to push");
            currentSize += 1;
            list.splice(list.end(), type, type.begin());
            if (currentSize == 1u)
                Pop.notify_one();
        }
    }
    QueueResult pop(T& var)
    {
        decltype(list) type;
        {
            std::unique_lock<std::mutex> lock(mutex);
            Pop.wait(lock);
            if (list.empty() && state == State::CLOSED)
                return CLOSED;
            currentSize -= 1;
            type.splice(type.begin(), list, list.begin());
            Push.notify_one();
        }
        var = type.front();
        return ENTER;
        }
        void close() noexcept
        {
        std::unique_lock<std::mutex> lock(mutex);
        state = State::CLOSED;
        Pop.notify_all();
        }
};
int main()
{
    const unsigned _ThreadsCount = 5;
    const unsigned num = 3;
    std::vector<std::thread> producers,consumers;
    std::mutex _Mutex;
    std::atomic<int> x(0);
    ThreadSafeQueue<int> queue(6);
    for(int i = 0; i < _ThreadsCount;++i)
        producers.push_back(std::thread([&,i]() {
            for (int j = 0; j < num; ++j) {

                {
                    std::lock_guard<std::mutex> lock(_Mutex);
                    std::cout << "Thread NO " << i << " pushing " << i * _ThreadsCount + j << " Into Queue\n";
                }
                queue.push(i * _ThreadsCount + j);
            }
        }));
    for(int i = _ThreadsCount;i < 2* _ThreadsCount;++i)
    consumers.push_back(std::thread([&,i]()
    {
        int j = -1;
        ThreadSafeQueue<int>::QueueResult result;
        while((result = queue.pop(j)) != ThreadSafeQueue<int>::CLOSED)
        {
            std::lock_guard<std::mutex> lock(_Mutex);
            std::cout<<"Thread NO "<< i << " got: "<< j<< " from Queue\n";
        }
        {
           std::lock_guard<std::mutex> lock(_Mutex);
           std::cout<<"Thread NO "<< i << " is execute\n";
        }
    }));
    {
        std::lock_guard<std::mutex> lock(_Mutex);
        std::cout << "Queue is closing.\n";
    }
    for (auto & t : producers)
        t.join();
    {
        std::lock_guard<std::mutex> lock(_Mutex);
        std::cout << "Queue is closing.\n";
    }
    queue.close();

    {
        std::lock_guard<std::mutex> lock(_Mutex);
        std::cout << "Waiting ...\n";
    }
    for (auto & t : consumers)
        t.join();
    return 0;
}
