#include "trigram_query.hpp"

std::vector<uint32_t> TrigramQuery::unverified_query(const std::string &query_string)
{
    std::vector<uint32_t> unique_trigrams;

    uint8_t *start = (uint8_t *)query_string.data(),
            *end = (uint8_t *)query_string.data() + query_string.size();

    TrigramBuilder::generate_unique_trigrams(unique_trigrams, start, end); // generate all unique trigrams

    if (unique_trigrams.size() < 1)
        throw std::runtime_error("not enough trigrams");

    std::vector<MappingIndex> trigrams_mappings;
    trigrams_mappings.reserve(unique_trigrams.size());

    for (auto trigram : unique_trigrams) // resolve all trigrams (use trigram value as index into mapping.bin generate above)
        trigrams_mappings.push_back(mapping[trigram]);

    std::sort(trigrams_mappings.begin(), trigrams_mappings.end(), [&](const MappingIndex &a, const MappingIndex &b) { // sort them from least to most common
        return a.count < b.count;
    });

    std::cout << "num trigrams: " << unique_trigrams.size() << std::endl;

    std::vector<uint32_t> line_options;

    for (auto trigram : trigrams_mappings)
    {
        uint32_t *start = index + trigram.offset,
                 *end = start + trigram.count;

        if (line_options.size() == 0)
        {
            line_options.assign(start, end);
        }
        else
        {
            std::vector<uint32_t> intersection;
            intersection.reserve(line_options.size());

            std::set_intersection(start, end, line_options.begin(), line_options.end(), std::back_inserter(intersection));

            std::cout << "reduced from " << line_options.size() << " to " << intersection.size() << std::endl;

            std::swap(intersection, line_options);
        }

        if (line_options.size() <= 1)
            break;
    }

    return line_options;
}

std::vector<uint64_t> TrigramQuery::query(const std::string &query_string, const MemoryMappedFile<uint8_t> &input_file){
    std::vector<uint32_t> line_options = this->unverified_query(query_string);

    std::vector<uint64_t> lines_found;
    lines_found.reserve(line_options.size());

    for (auto line_id : line_options)
    {
        uint64_t start_offset = this->offsets[line_id], end_offset = this->offsets[line_id + 1];
        const uint8_t *start_ptr = input_file.begin() + start_offset,
                      *end_ptr = input_file.begin() + end_offset;
        if (std::search(start_ptr, end_ptr, query_string.begin(), query_string.end()) != end_ptr)
            lines_found.push_back(ids.at(line_id));
    }

    std::cout << "Found " << lines_found.size() << " actual results" << std::endl;

    return lines_found;
}

std::vector<std::string> TrigramQuery::query_with_results(const std::string &query_string, const MemoryMappedFile<uint8_t> &input_file) {
    std::vector<uint32_t> line_options = this->unverified_query(query_string);

    std::vector<std::string> lines_found;
    lines_found.reserve(line_options.size());

    for (auto line_id : line_options)
    {
        uint64_t start_offset = this->offsets[line_id], end_offset = this->offsets[line_id + 1];
        const uint8_t *start_ptr = input_file.begin() + start_offset,
                      *end_ptr = input_file.begin() + end_offset;
        if (std::search(start_ptr, end_ptr, query_string.begin(), query_string.end()) != end_ptr) {
            std::string line(start_ptr, end_ptr);
            lines_found.push_back(line);
        }
    }

    std::cout << "Found " << lines_found.size() << " actual results" << std::endl;

    return lines_found;
}