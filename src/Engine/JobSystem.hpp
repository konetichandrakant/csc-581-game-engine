#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstddef>   // size_t

// Forward-declare JobQueue so this header doesn't depend on its definition.
// The actual definition must be visible in JobSystem.cpp via an #include.
struct JobQueue;

// Minimal shared state used by worker threads.
// Add fields here if your worker uses more (e.g., stop flags, counters).
struct SharedData {
    std::mutex m;
    std::condition_variable cv;

    std::atomic<bool>   running{true};

    // Add the fields your worker uses:
    std::atomic<std::size_t> nextJobIndex{0};
    std::size_t              totalJobs{0};
};

// Keep your existing declaration exactly the same (only types made visible now)
void worker(SharedData& data, const JobQueue& jobs);
