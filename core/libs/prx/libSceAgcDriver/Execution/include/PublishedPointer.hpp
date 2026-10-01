#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PUBLISHEDPOINTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PUBLISHEDPOINTER_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace AgcDriver {

template<typename TValue>
class PublishedPointer {
public:
    struct Cache {
        std::shared_ptr<TValue> value;
        std::uint64_t generation = 0;
    };

    std::shared_ptr<TValue> Get() const {
        std::lock_guard lock(mutex);
        return value;
    }

    const std::shared_ptr<TValue>& Get(Cache& cache) const {
        if (generation.load(std::memory_order_acquire) == cache.generation) return cache.value;
        auto previous = std::move(cache.value);
        std::lock_guard lock(mutex);
        cache.value = value;
        cache.generation = generation.load(std::memory_order_relaxed);
        return cache.value;
    }

    template<typename TCreate>
    const std::shared_ptr<TValue>& GetOrCreate(Cache& cache, TCreate&& create) {
        if (Get(cache) != nullptr) return cache.value;
        auto created = create();
        std::lock_guard lock(mutex);
        if (value == nullptr) {
            value = std::move(created);
            generation.store(nextGeneration(), std::memory_order_release);
        }
        cache.value = value;
        cache.generation = generation.load(std::memory_order_relaxed);
        return cache.value;
    }

    void Publish(std::shared_ptr<TValue> replacement) {
        std::lock_guard lock(mutex);
        value.swap(replacement);
        generation.store(nextGeneration(), std::memory_order_release);
    }

private:
    static std::uint64_t nextGeneration() {
        static std::atomic<std::uint64_t> generations{0};
        return generations.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    mutable std::mutex mutex;
    std::shared_ptr<TValue> value;
    std::atomic<std::uint64_t> generation{0};
};

}

#endif
