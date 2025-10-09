// Engine/Jobs.hpp
#pragma once
#include <vector>
#include <functional>
#include <cstddef> // size_t

struct JobQueue {
    using Job = std::function<void()>;   // jobs are callable with no args

    std::vector<Job> items;

    // Allow jobs[j] syntax:
    const Job& operator[](std::size_t i) const { return items[i]; }
    Job&       operator[](std::size_t i)       { return items[i]; }

    // Size query used by the worker:
    std::size_t size() const noexcept { return items.size(); }

    // Optional helper to add jobs:
    void push(Job j) { items.emplace_back(std::move(j)); }
};
