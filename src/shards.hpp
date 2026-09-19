#ifndef SHARDS_H
#define SHARDS_H

#include <mutex>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <filesystem>

#include "definitions.hpp"


template <typename T, int size>
class ShardBuffer
{
    std::vector<T> buffer;

    std::mutex &mutex;

    int fd;

public:
    ShardBuffer() : buffer(), fd() {};

    ShardBuffer(int fd, std::mutex &mutex) : buffer(), fd(fd), mutex(mutex)
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
        std::lock_guard lock(mutex);
        if (buffer.size())
            this->write_all(this->fd, buffer.data(), buffer.size() * sizeof(T));
    }

    void write_all(int fd, const void *buf, size_t n)
    {
        size_t num_written = 0;

        for (int attempts = 0; attempts < 10 && num_written < n; attempts++)
            num_written += ::write(fd, (uint8_t *)buf + num_written, n - num_written);

        if (num_written != n)
            std::runtime_error("failed to write full content after 10 attempts");
    }
};

template <typename T, int buffer_size>
class ShardsManager
{
    using Shard = ShardBuffer<T, buffer_size>;
    using ShardsBuffers = std::vector<Shard>;

    std::vector<int> fds;
    std::vector<std::mutex> mutexes;

    std::string base_directory = "/mnt/tmpfs/shards/";

public:
    std::vector<int> non_empty_shards;
    std::vector<std::filesystem::path> files;

    ShardsManager() : mutexes(NUM_SHARDS)
    {
        std::filesystem::create_directory(this->base_directory);

        for (int i = 0; i < NUM_SHARDS; i++)
        {
            std::string filename = this->base_directory + "shard" + std::to_string(i) + ".bin";
            this->files.push_back(filename);

            int fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_APPEND | O_TRUNC, 0666);

            if (fd < 0)
            {
                std::perror("open");
                throw std::runtime_error("open " + filename);
            }

            this->fds.push_back(fd);
        }
    }

    ShardsBuffers create_shard_buffers()
    {
        ShardsBuffers shards;

        for (int i = 0; i < NUM_SHARDS; i++)
            shards.push_back(Shard(this->fds[i], this->mutexes[i]));

        return shards;
    }

    void close()
    {
        for (auto fd : this->fds)
            ::close(fd);
    }

    uint64_t find_non_empty_shards()
    {
        for (int i = 0; i < NUM_SHARDS; i++)
            if (!std::filesystem::is_empty(this->files[i]))
                this->non_empty_shards.push_back(i);

        return this->non_empty_shards.size();
    }

    void cleanup()
    {
        // std::filesystem::remove_all(this->base_directory);
    }
};

#endif