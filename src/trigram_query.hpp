#ifndef TRIGRAM_QUERY_H
#define TRIGRAM_QUERY_H

#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

#include "structs.hpp"
#include "trigram_builder.hpp"
#include "memory_mapped_file.hpp"


class TrigramQuery
{
    MemoryMappedFile<uint8_t> content;
    MemoryMappedFile<MappingIndex> mapping;
    MemoryMappedFile<uint32_t> index;
    MemoryMappedFile<uint64_t> offsets;
    MemoryMappedFile<uint64_t> ids;

public:
    TrigramQuery() : mapping("mapping.bin"), index("index.bin"), offsets("offsets.bin"), ids("ids.bin") {}

    std::vector<std::string> query_with_results(const std::string &query_string, const MemoryMappedFile<uint8_t> &input_file);

    std::vector<uint32_t> unverified_query(const std::string &query_string);

    std::vector<uint64_t> query(const std::string &query_string, const MemoryMappedFile<uint8_t> &input_file);
};

#endif