#pragma once

#include <algorithm>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>
#include <condition_variable>
#include <mutex>

#include <fcntl.h>
#include <unistd.h>

#include "memory_mapped_file.hpp"
#include "structs.hpp"
#include "shardbuffer.hpp"
#include "definitions.hpp"
#include "progress.hpp"
#include "thread_pool.hpp"
#include "shard.hpp"


class TrigramBuilder
{
public:
    TrigramBuilder() {}

    static void build(bool build_ids_file);

    static void generate_unique_trigrams(std::vector<uint32_t> &grams, uint8_t *buffer, uint8_t *end);

    static uint32_t build_trigram(uint8_t *buffer);

    static uint64_t compute_line_boundaries(MemoryMappedFile<uint8_t> &input_file, std::vector<uint64_t> &line_boundaries);

private:

    static void create_mapping(MemoryMappedFile<MappingIndex> &mapping);

    static void create_shards(MemoryMappedFile<uint8_t> &input_file, std::vector<u_int64_t> &line_boundaries, uint64_t longest_line, ShardsManager<ShardIndexPair> &shards_manager);

    static void process_shards(ShardsManager<ShardIndexPair> &shards_manager, MemoryMappedFile<MappingIndex> &mapping);

    static void create_offsets_file(std::vector<uint64_t> &line_boundaries);

    static size_t get_longest_line(std::vector<uint64_t> &line_boundaries);

    static void replace_active_index(MemoryMappedFile<uint8_t> &input_file);

    static void build_ids(std::vector<uint64_t> &line_boundaries);
};