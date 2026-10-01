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
        return value.load(std::memory_order_acquire);
    }

    const std::shared_ptr<TValue>& Get(Cache& cache) const {
        const auto current = generation.load(std::memory_order_acquire);
        if (current == cache.generation) return cache.value;
        cache.value = value.load(std::memory_order_acquire);
        cache.generation = current;
        return cache.value;
    }

    template<typename TCreate>
    const std::shared_ptr<TValue>& GetOrCreate(Cache& cache, TCreate&& create) {
        if (Get(cache) != nullptr) return cache.value;
        auto created = create();
        std::lock_guard lock(mutex);
        if (value.load(std::memory_order_acquire) == nullptr) {
            value.store(std::move(created), std::memory_order_release);
            generation.store(nextGeneration(), std::memory_order_release);
        }
        cache.value = value.load(std::memory_order_acquire);
        cache.generation = generation.load(std::memory_order_acquire);
        return cache.value;
    }

    void Publish(std::shared_ptr<TValue> replacement) {
        std::lock_guard lock(mutex);
        value.store(std::move(replacement), std::memory_order_release);
        generation.store(nextGeneration(), std::memory_order_release);
    }

private:
    static std::uint64_t nextGeneration() {
        static std::atomic<std::uint64_t> generations{0};
        return generations.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    std::mutex mutex;
    std::atomic<std::shared_ptr<TValue>> value;
    std::atomic<std::uint64_t> generation{0};
};

}

#endif
