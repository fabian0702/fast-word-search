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

struct MunmapDeleter
{
    std::size_t bytes = 0;

    void operator()(void *ptr) const noexcept
    {
        if (ptr)
            munmap(ptr, bytes);
    }
};

template <typename T>
using Mapping = std::unique_ptr<T, MunmapDeleter>;

template <typename T>
class Shard
{
    static constexpr std::size_t CHUNK_SIZE = 16 * 1024 * 1024;
    static constexpr std::size_t CHUNK_CAPACITY = CHUNK_SIZE / sizeof(T);

    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(CHUNK_SIZE % sizeof(T) == 0);

    std::vector<T *> chunks;

    std::size_t _size = 0;

    std::mutex grow_mutex;

    void grow()
    {
        T *new_chunk = (T *)mmap(
            nullptr,
            CHUNK_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0);

        if (new_chunk == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("failed to mmap new chunk");
        }

        if (madvise(new_chunk, CHUNK_SIZE, MADV_POPULATE_WRITE) != 0)
        {
            munmap(new_chunk, CHUNK_SIZE);
            std::perror("madvise");
            throw std::runtime_error("failed to madvise new chunk");
        }

        try
        {
            chunks.push_back(new_chunk);
        }
        catch (...)
        {
            munmap(new_chunk, CHUNK_SIZE);
            throw;
        }
    }

public:
    Shard() {}

    Shard(const Shard &) = delete;
    Shard &operator=(const Shard &) = delete;

    Mapping<T> consolidate()
    {
        if (this->chunks.size() == 0)
            return Mapping<T>(nullptr);

        std::size_t consolidated_size = this->chunks.size() * CHUNK_SIZE;

        std::byte *consolidated_ptr = (std::byte *)mmap(
            NULL,
            consolidated_size,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);

        if (consolidated_ptr == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("failed to mmap consolidated area");
        }

        for (int i = 0; i < this->chunks.size(); i++)
        {
            void *relocated = mremap(
                this->chunks[i],
                CHUNK_SIZE,
                CHUNK_SIZE,
                MREMAP_FIXED | MREMAP_MAYMOVE,
                consolidated_ptr + (i * CHUNK_SIZE));

            if (relocated == MAP_FAILED)
            {
                std::perror("mremap");
                throw std::runtime_error("failed to remap shard chunk");
            }

            chunks[i] = static_cast<T *>(relocated);
        }

        this->chunks.clear();

        return Mapping<T>((T *)consolidated_ptr, MunmapDeleter{consolidated_size});
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

            this->chunks[chunk_index][chunk_offset] = obj[i];
        }

        this->_size += required_size;
    }

    std::size_t size()
    {
        return this->_size;
    }

    ~Shard()
    {
        for (auto chunk : this->chunks)
            munmap(chunk, CHUNK_SIZE);
    }
};