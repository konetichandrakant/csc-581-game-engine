#include "MemoryManager.hpp"

namespace Engine::Memory {

    MemoryManager& MemoryManager::instance() {
        static MemoryManager manager;
        return manager;
    }

    void MemoryManager::clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pools_.clear();
    }

}
