#include "JobSystem.hpp"
#include <cstddef>
#include "Jobs.hpp"

// Worker thread for jobs
void worker(SharedData& data, const JobQueue& jobs) {
    while (true) {
        size_t jobIndex = data.nextJobIndex.fetch_add(1);
        if (jobIndex >= jobs.size()) {
            break;
        }
        jobs[jobIndex]();
    }
}
