#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Engine::Memory {

    struct PoolStats {
        std::size_t capacity = 0;
        std::size_t active = 0;
        std::size_t overflow = 0;

        std::size_t available() const noexcept {
            return (capacity > active) ? capacity - active : 0;
        }
    };

    template <typename T>
    class PoolAllocator {
        public:
            PoolAllocator() = default;
            explicit PoolAllocator(std::size_t capacity) {
                configure(capacity);
            }

            void configure(std::size_t capacity) {
                nodes_.clear();
                nodes_.resize(capacity);
                freeList_ = nullptr;
                for (auto& node : nodes_) {
                    node.next = freeList_;
                    node.occupied = false;
                    freeList_ = &node;
                }
                active_ = 0;
                overflow_ = 0;
                overflowAllocations_.clear();
            }

            template <typename... Args>
            T* create(Args&&... args) {
                if (freeList_) {
                    Node* node = freeList_;
                    freeList_ = node->next;
                    node->occupied = true;
                    T* instance = reinterpret_cast<T*>(&node->storage);
                    new (instance) T(std::forward<Args>(args)...);
                    ++active_;
                    return instance;
                }
                T* heapAlloc = new T(std::forward<Args>(args)...);
                overflowAllocations_.insert(heapAlloc);
                ++overflow_;
                return heapAlloc;
            }

            void destroy(T* object) {
                if (!object) return;

                auto overflowIt = overflowAllocations_.find(object);
                if (overflowIt != overflowAllocations_.end()) {
                    overflowAllocations_.erase(overflowIt);
                    delete object;
                    return;
                }

                if (!owns(object)) {
                    delete object;
                    return;
                }

                object->~T();
                Node* node = reinterpret_cast<Node*>(object);
                node->occupied = false;
                node->next = freeList_;
                freeList_ = node;
                if (active_ > 0) --active_;
            }

            bool owns(const T* object) const noexcept {
                if (nodes_.empty()) return false;
                auto* begin = reinterpret_cast<const unsigned char*>(nodes_.data());
                auto* end = begin + nodes_.size() * sizeof(Node);
                auto* ptr = reinterpret_cast<const unsigned char*>(object);
                return ptr >= begin && ptr < end;
            }

            std::size_t capacity() const noexcept { return nodes_.size(); }
            std::size_t inUse() const noexcept { return active_; }
            std::size_t overflowCount() const noexcept { return overflow_; }

        private:
            struct Node {
                typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
                Node* next = nullptr;
                bool occupied = false;
            };

            Node* freeList_ = nullptr;
            std::vector<Node> nodes_;
            std::unordered_set<T*> overflowAllocations_;
            std::size_t active_ = 0;
            std::size_t overflow_ = 0;
    };

    class MemoryManager {
        public:
            static MemoryManager& instance();

            MemoryManager(const MemoryManager&) = delete;
            MemoryManager& operator=(const MemoryManager&) = delete;

            template <typename T>
            void configurePool(std::size_t capacity) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto holder = std::make_unique<PoolHolder<T>>(capacity);
                pools_[std::type_index(typeid(T))] = std::move(holder);
            }

            template <typename T, typename... Args>
            T* create(Args&&... args) {
                PoolHolder<T>* holder = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = pools_.find(std::type_index(typeid(T)));
                    if (it != pools_.end()) {
                        holder = static_cast<PoolHolder<T>*>(it->second.get());
                    }
                }
                if (holder) {
                    return holder->pool.create(std::forward<Args>(args)...);
                }
                return new T(std::forward<Args>(args)...);
            }

            template <typename T>
            void destroy(T* object) {
                if (!object) return;
                PoolHolder<T>* holder = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = pools_.find(std::type_index(typeid(T)));
                    if (it != pools_.end()) {
                        holder = static_cast<PoolHolder<T>*>(it->second.get());
                    }
                }
                if (holder) {
                    holder->pool.destroy(object);
                } else {
                    delete object;
                }
            }

            template <typename T>
            PoolStats stats() const {
                std::lock_guard<std::mutex> lock(mutex_);
                PoolStats stats{};
                auto it = pools_.find(std::type_index(typeid(T)));
                if (it != pools_.end()) {
                    auto* holder = static_cast<PoolHolder<T>*>(it->second.get());
                    stats.capacity = holder->pool.capacity();
                    stats.active = holder->pool.inUse();
                    stats.overflow = holder->pool.overflowCount();
                }
                return stats;
            }

            void clear();

        private:
            MemoryManager() = default;
            ~MemoryManager() = default;

            struct IPoolHolder {
                virtual ~IPoolHolder() = default;
            };

            template <typename T>
            struct PoolHolder : IPoolHolder {
                explicit PoolHolder(std::size_t capacity) : pool(capacity) {}
                PoolAllocator<T> pool;
            };

            mutable std::mutex mutex_;
            std::unordered_map<std::type_index, std::unique_ptr<IPoolHolder>> pools_;
    };

    template <typename T>
    struct PoolDeleter {
        void operator()(T* object) const noexcept {
            MemoryManager::instance().destroy(object);
        }
    };

} // namespace Engine::Memory
