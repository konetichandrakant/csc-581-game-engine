#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstddef>



struct JobQueue;



struct SharedData {
    std::mutex m;
    std::condition_variable cv;

    std::atomic<bool>   running{true};


    std::atomic<std::size_t> nextJobIndex{0};
    std::size_t              totalJobs{0};
};


void worker(SharedData& data, const JobQueue& jobs);
