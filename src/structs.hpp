#ifndef STRUCTS_H
#define STRUCTS_H

#include <cstdint>
#include <iostream>

#define SHARD_OFFSET_MASK 0xffff


struct ShardIndexPair
{
    uint16_t shard_offset;
    uint32_t line_id;
    uint16_t pad;

    friend std::ostream &operator<<(std::ostream &ostream, ShardIndexPair &obj)
    {
        ostream << "shard_offset: " << obj.shard_offset << " line_id: " << obj.line_id << " pad: " << obj.pad;
        return ostream;
    }
} __attribute__((packed));

struct MappingIndex
{
    uint64_t offset;
    uint32_t count;
    uint32_t pad;

    friend std::ostream &operator<<(std::ostream &ostream, MappingIndex &obj)
    {
        ostream << "offset: " << obj.offset << " count: " << obj.count << " pad: " << obj.pad;
        return ostream;
    }
} __attribute__((packed));

#endif