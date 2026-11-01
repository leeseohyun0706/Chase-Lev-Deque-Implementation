
#include "deque.h++"
#include <iostream>
#include <thread>
// #include <syncstream> // It's not found in my environment

template<int N>
int num() { return N; }

class PureTask {
private:
public:
    int index;

    int execute() const {
        return index * 3;
    }
};


int main() {

    Deque<PureTask> deque;
    std::atomic<int> done{0};

    constexpr int N = 1000;
    constexpr int THREADS = 4;

    PureTask tasks[N];
    for (int i = 0; i < N; i++) {
        tasks[i] = PureTask();
        tasks[i].index = i;
    }
    std::vector<std::thread> threads;
    threads.reserve(THREADS);

    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back([&]() {
            while (done.load(std::memory_order_relaxed) != N) {
                if (auto* task = deque.steal()) {
                    std::cout << "Task " << task->index << " run : " << task->execute() << "\n"; // I need to use or make async logging library based on lock-free queue
                    done.fetch_add(1, std::memory_order_relaxed);
                }
            }
            std::cout << "Thread End : " << i << "\n";
            std::this_thread::yield();
        });
    }

    for (auto & task : tasks) {
        deque.push(&task);
    }

    while (done.load(std::memory_order_relaxed) != N) {
        if (auto* task = deque.pop()) {
            std::cout << "Task " << task->index << " run : " << task->execute() << "\n";
            done.fetch_add(1, std::memory_order_relaxed);
        }
    }
    std::this_thread::yield();
    for (auto& task : threads) {
        task.join();
    }
    std::cout << "Done : " << done.load() << "\n";
}

