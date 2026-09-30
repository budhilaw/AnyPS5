#define _GLIBCXX_HAS_GTHREADS 0
#include "MemoryPool.hpp"
#include "DirectMemory.hpp"
#include <mutex>

static constexpr size_t NUM_PAGES = DIRECT_MEMORY_SIZE / PS5_PAGE_SIZE;

struct PhysicalMemoryPool {
    static PhysicalMemoryPool& Instance() {
        static PhysicalMemoryPool inst;
        return inst;
    }

    int Alloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t align = (alignment == 0) ? PS5_PAGE_SIZE : alignment;
        uint64_t start = static_cast<uint64_t>(searchStart);
        uint64_t end = static_cast<uint64_t>(searchEnd);
        uint64_t cur = (start + align - 1) & ~(align - 1);
        while (cur + len <= end && cur + len <= DIRECT_MEMORY_SIZE) {
            if (_isFree(cur, len)) {
                _mark(cur, len, static_cast<int8_t>(memoryType));
                *physOut = static_cast<int64_t>(cur);
                return 0;
            }
            cur += align;
        }
        return SCE_KERNEL_ERROR_EAGAIN;
    }

    void Free(uint64_t start, size_t len) {
        std::lock_guard<std::mutex> lock(_mutex);
        _mark(start, len, Free_);
    }

    bool Query(uint64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t page = offset / PS5_PAGE_SIZE;
        if (findNext)
            while (page < NUM_PAGES && _type[page] == Free_) ++page;
        if (page >= NUM_PAGES || _type[page] == Free_) return false;
        const auto type = _type[page];
        size_t first = page;
        while (first > 0 && _type[first - 1] == type) --first;
        size_t last = page + 1;
        while (last < NUM_PAGES && _type[last] == type) ++last;
        *start = static_cast<int64_t>(first * PS5_PAGE_SIZE);
        *end = static_cast<int64_t>(last * PS5_PAGE_SIZE);
        *memoryType = type;
        return true;
    }

private:
    bool _isFree(uint64_t offset, size_t len) const {
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        for (size_t i = 0; i < count; ++i)
            if (_type[first + i] != Free_) return false;
        return true;
    }

    void _mark(uint64_t offset, size_t len, int8_t type) {
        size_t first = offset / PS5_PAGE_SIZE;
        size_t count = len / PS5_PAGE_SIZE;
        for (size_t i = 0; i < count; ++i)
            _type[first + i] = type;
    }

    static constexpr int8_t Free_ = -1;
    std::mutex _mutex;
    int8_t _type[NUM_PAGES] = {};

public:
    PhysicalMemoryPool() {
        for (auto& type : _type) type = Free_;
    }
};

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut) {
    return PhysicalMemoryPool::Instance().Alloc(searchStart, searchEnd, len, alignment, memoryType, physOut);
}

bool DirectMemoryQueryRange(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType) {
    return PhysicalMemoryPool::Instance().Query(static_cast<uint64_t>(offset), findNext, start, end, memoryType);
}

void DirectMemoryFree(int64_t start, size_t len) {
    PhysicalMemoryPool::Instance().Free(static_cast<uint64_t>(start), len);
}
