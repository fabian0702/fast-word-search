#pragma once

#include <vector>
#include <sys/mman.h>
#include <exception>
#include <cstdio>
#include <stdexcept>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <mutex>

constexpr std::size_t round_up(std::size_t value, std::size_t multiple)
{
    auto rem = value % multiple;
    return rem == 0 ? value : value + multiple - rem;
}

template <typename T>
class Mapping {
    std::size_t _size;
    std::size_t mapped_size;

public:
    void *ptr;

    Mapping() : _size(0), ptr(nullptr), mapped_size(0) {}

    Mapping(std::size_t size, int prot, int flags) : _size(size) {
        if (size == 0) {
            this->mapped_size = 0;
            this->ptr = nullptr;

            return;
        }

        this->mapped_size = round_up(size * sizeof(T), 0x1000);

        this->ptr = mmap(
            nullptr,
            this->mapped_size,
            prot,
            flags,
            -1,
            0);

        if (this->ptr == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("failed to mmap new chunk");
        }
    }


    ~Mapping() {
        if (this->ptr != nullptr)
            munmap(this->ptr, this->mapped_size);
    }

    T *begin() {
        return (T*)this->ptr;
    }

    T *end() {
        return (T*)this->ptr + this->_size;
    }

    T& at(std::size_t index) {
        return static_cast<T *>(this->ptr)[index];
    }

    std::size_t size() {
        return this->_size;
    }
};

template <typename T>
class Shard
{
    static constexpr std::size_t CHUNK_SIZE = 4096; //16 * 1024 * 1024;
    static constexpr std::size_t CHUNK_CAPACITY = CHUNK_SIZE / sizeof(T);

    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(CHUNK_SIZE % sizeof(T) == 0);

    std::vector<std::unique_ptr<Mapping<T>>> chunks;

    std::size_t _size = 0;

    std::mutex grow_mutex;

    void grow()
    {
        auto new_mapping = std::make_unique<Mapping<T>>(CHUNK_CAPACITY, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS);

        if (madvise(new_mapping->ptr, CHUNK_SIZE, MADV_POPULATE_WRITE) != 0)
        {
            std::perror("madvise");
            throw std::runtime_error("failed to madvise new chunk");
        }

        chunks.push_back(std::move(new_mapping));
    }

public:
    Shard() {}

    Shard(const Shard &) = delete;
    Shard &operator=(const Shard &) = delete;

    std::unique_ptr<Mapping<T>> consolidate()
    {
        if (this->chunks.size() == 0)
            return std::make_unique<Mapping<T>>();

        std::size_t consolidated_size = this->chunks.size() * CHUNK_CAPACITY;

        auto consolidated_mapping = std::make_unique<Mapping<T>>(consolidated_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS);

        for (int i = 0; i < this->chunks.size(); i++)
        {
            void *relocated = mremap(
                this->chunks[i]->ptr,
                CHUNK_SIZE,
                CHUNK_SIZE,
                MREMAP_FIXED | MREMAP_MAYMOVE,
                (std::byte *)consolidated_mapping->ptr + (i * CHUNK_SIZE));

            if (relocated == MAP_FAILED)
            {
                std::perror("mremap");
                throw std::runtime_error("failed to remap shard chunk");
            }

            this->chunks[i]->ptr = nullptr;
        }

        this->chunks.clear();

        return consolidated_mapping;
    }

    void push(const std::vector<T> &obj)
    {
        std::unique_lock grow_lock(this->grow_mutex);

        if (obj.empty())
            return;

        const std::size_t start = this->_size;
        const std::size_t required_size = start + obj.size();

        while (required_size > this->chunks.size() * CHUNK_CAPACITY)
            this->grow();

        for (std::size_t i = 0; i < obj.size(); i++)
        {
            const std::size_t index = start + i;

            const std::size_t chunk_index = index / CHUNK_CAPACITY;
            const std::size_t chunk_offset = index % CHUNK_CAPACITY;

            this->chunks[chunk_index]->at(chunk_offset) = obj[i];
        }

        this->_size = required_size;
    }

    std::size_t size()
    {
        return this->_size;
    }
};