#ifndef PROGRESS_H
#define PROGRESS_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <iostream>
#include <iomanip>

class Progress
{
    std::string label_;
    uint64_t total_;
    uint64_t next_ = 0;
    uint64_t last_ = 0;
    std::mutex mutex_;
    uint64_t done_ = 0;

public:
    uint64_t quantum;

    Progress(std::string label, uint64_t total)
        : label_(std::move(label)), total_(total),
          quantum(std::max<uint64_t>(total / 1000, 1))
    {
        render(0);
    }
    void update(uint64_t done)
    {
        std::lock_guard lock(mutex_);
        if (done <= last_)
            return;
        if (done < next_ && done < total_)
            return;
        done = std::min(done, total_);
        next_ = std::min(total_, done + quantum);
        last_ = done;
        render(done);
    }

    void add(uint64_t offset)
    {
        std::lock_guard lock(mutex_);

        done_ = std::min(done_ + offset, total_);
        next_ = std::min(total_, done_ + quantum);
        last_ = done_;
        render(done_);
    }

    void add()
    {
        this->add(1);
    }

    void finish()
    {
        std::lock_guard lock(mutex_);
        if (last_ != total_)
            render(total_);
        std::cerr << '\n';
    }

private:
    void render(uint64_t done) const
    {
        constexpr unsigned width = 40;
        const double ratio = total_ ? static_cast<double>(done) / total_ : 1.0;
        const unsigned filled = static_cast<unsigned>(ratio * width);
        std::cerr << '\r' << std::left << std::setw(9) << label_ << " [";
        for (unsigned i = 0; i < width; ++i)
            std::cerr << (i < filled ? '=' : (i == filled && filled < width ? '>' : ' '));
        std::cerr << "] " << std::right << std::setw(6) << std::fixed
                  << std::setprecision(1) << ratio * 100.0 << '%' << std::flush;
    }
};

#endif