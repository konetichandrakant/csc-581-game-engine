
#pragma once
#include <vector>
#include <functional>
#include <cstddef> // size_t

struct JobQueue {
    using Job = std::function<void()>;   // jobs are callable with no args

    std::vector<Job> items;

    const Job& operator[](std::size_t i) const { return items[i]; }
    Job&       operator[](std::size_t i)       { return items[i]; }

    std::size_t size() const noexcept { return items.size(); }

    void push(Job j) { items.emplace_back(std::move(j)); }
};
