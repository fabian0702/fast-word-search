#pragma once

#include <mutex>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <filesystem>

#include "definitions.hpp"
#include "shard.hpp"

template <typename T, typename FlushableType, int size>
class FlushableBuffer
{
    std::vector<T> buffer;

    FlushableType &sink;

public:
    FlushableBuffer(FlushableType &sink) : sink(sink)
    {
        buffer.reserve(size);
    }

    void push(T obj)
    {
        this->buffer.push_back(obj);

        if (this->buffer.size() > size)
        {
            this->flush();
            buffer.clear();
        }
    }

    void flush()
    {
        this->sink.push(this->buffer);
    }
};

template <typename T>
using ShardBuffer = FlushableBuffer<T, Shard<T>, 1 << 18>;

template <typename T>
using ShardsBuffers = std::vector<ShardBuffer<T>>;

template <typename T>
class ShardsManager
{
public:
    std::vector<Shard<T>> shards;

    std::vector<int> non_empty_shards;

    ShardsManager() : shards(NUM_SHARDS) {}

    ShardsBuffers<T> create_shard_buffers()
    {
        ShardsBuffers<T> buffers;

        for (int i = 0; i < NUM_SHARDS; i++)
            buffers.push_back(ShardBuffer(this->shards[i]));

        return buffers;
    }

    uint64_t find_non_empty_shards()
    {
        for (int i = 0; i < NUM_SHARDS; i++)
            if (this->shards[i].size() != 0)
                this->non_empty_shards.push_back(i);

        return this->non_empty_shards.size();
    }
};