#pragma once

#include <vector>
#include <thread>
#include <iostream>

template <typename F>
class ThreadingPool
{
    std::vector<std::thread> threads;

public:
    ThreadingPool(uint64_t total_work, const F &callable)
    {
        int num_threads = std::max(1u, std::thread::hardware_concurrency());

        // std::cout << "threads: " << num_threads << std::endl;

        this->threads.reserve(num_threads);

        // float quantum_per_worker = float(total_work) / num_threads;

        for (int worker_id = 0; worker_id < num_threads; worker_id++)
            this->threads.emplace_back(callable, worker_id, total_work, num_threads);
    }

    void join()
    {
        for (int i = 0; i < this->threads.size(); i++)
            this->threads[i].join();
    }
};