#ifndef MEMORY_MAPPED_FILE_H
#define MEMORY_MAPPED_FILE_H

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <src/progress.hpp>

template <typename T>
class MemoryMappedFile
{
    int fd;
    struct stat st;

    uint64_t _size;
    T *buffer;

public:
    MemoryMappedFile() : fd(-1) {}

    MemoryMappedFile(const std::string &path, size_t count) : _size(count)
    {
        this->fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (this->fd < 0)
        {
            std::perror("open");
            throw std::runtime_error("open " + path);
        }

        if (::ftruncate(fd, static_cast<off_t>(count * sizeof(T))) == -1)
        {
            std::perror("ftruncate");
            throw std::runtime_error("could not resize " + path);
        }

        this->buffer = (T *)::mmap(nullptr, count * sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);
        if (this->buffer == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("mmap " + path);
        }
    }

    MemoryMappedFile(size_t count)
    {
        this->fd = -1;

        this->buffer = (T *)::mmap(0x0, count * sizeof(T), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (this->buffer == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("mmap ");
        }
    }

    MemoryMappedFile(const std::string &path) : st({})
    {
        this->fd = ::open(path.c_str(), O_RDWR);

        if (this->fd < 0)
        {
            std::perror("open");
            throw std::runtime_error("open " + path);
        }

        if (fstat(fd, &this->st))
        {
            std::perror("fstat");
            throw std::runtime_error("fstat " + path);
        }

        this->_size = this->st.st_size / sizeof(T);

        this->buffer = (T *)::mmap(0x0, this->st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (this->buffer == MAP_FAILED)
        {
            std::perror("mmap");
            throw std::runtime_error("mmap " + path);
        }
    }

    ~MemoryMappedFile()
    {
        ::munmap((void *)this->buffer, this->_size * sizeof(T));
        ::close(this->fd);
    }

    T &operator[](size_t index)
    {
        return this->buffer[index];
    }

    T &at(size_t index)
    {
        if (index > this->_size)
            throw std::runtime_error("index too large");

        return this->buffer[index];
    }

    T *operator+(size_t offset)
    {
        return this->buffer + offset;
    }

    T *begin()
    {
        return this->buffer;
    }

    T *end()
    {
        return this->buffer + this->_size;
    }

    const T *begin() const noexcept
    {
        return this->buffer;
    }

    const T *end() const noexcept
    {
        return this->buffer + this->_size;
    }

    size_t size()
    {
        return this->_size;
    }

    void copy_content(const char *new_file, bool should_replace) {
        int new_fd = ::open(new_file, O_RDWR | O_CREAT | (should_replace ? O_TRUNC : O_APPEND), 0666);

        size_t amount_copied = 0, total_size = this->_size * sizeof(T);

        Progress pb_copy_file("copy file", total_size);

        while (amount_copied < total_size)
        {
            ssize_t n = ::write(new_fd, (uint8_t *)this->buffer + amount_copied, total_size - amount_copied);
            if (n  < 0) {
                std::perror("write");
                throw std::runtime_error("write failed");
            }
            pb_copy_file.add(n);
        }
        pb_copy_file.finish()
    }
};

#endif