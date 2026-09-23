#include "trigram_builder.hpp"

void TrigramBuilder::build(bool build_ids_file)
{
    MemoryMappedFile<uint8_t> input_file("content.bin.new");

    std::vector<uint64_t> line_boundaries;
    uint64_t num_lines = TrigramBuilder::compute_line_boundaries(input_file, line_boundaries);

    size_t longest_line = TrigramBuilder::get_longest_line(line_boundaries);
    TrigramBuilder::create_offsets_file(line_boundaries);

    ShardsManager<ShardIndexPair> shards_manager;
    TrigramBuilder::create_shards(input_file, line_boundaries, longest_line, shards_manager);

    MemoryMappedFile<MappingIndex> mapping("mapping.bin.new", MAX_TRIGRAMS);
    TrigramBuilder::process_shards(shards_manager, mapping);

    TrigramBuilder::create_mapping(mapping);

    if (build_ids_file)
        TrigramBuilder::build_ids(line_boundaries);

    TrigramBuilder::replace_active_index(input_file);
}

void TrigramBuilder::build_ids(std::vector<uint64_t> &line_boundaries) {
    MemoryMappedFile<uint64_t> ids_file("ids.bin.new", line_boundaries.size());

    Progress pb_create_ids("creating line -> ids mapping", line_boundaries.size());

    for (uint64_t i = 0; i < line_boundaries.size(); i++) {
        ids_file[i] = i;
        
        if (i % pb_create_ids.quantum == 0)
            pb_create_ids.update(i);
    }

    pb_create_ids.finish();
}

void TrigramBuilder::create_mapping(MemoryMappedFile<MappingIndex> &mapping)
{
    Progress pb_create_mapping("creating mapping", MAX_TRIGRAMS);

    uint64_t total_count = 0;
    for (uint32_t gram_id = 0; gram_id < MAX_TRIGRAMS; gram_id++)
    {
        if (gram_id % pb_create_mapping.quantum == 0)
            pb_create_mapping.update(gram_id);
            
        uint64_t current_count = mapping[gram_id].count;
        if (current_count)
        {
            mapping[gram_id].offset = total_count;
            total_count += current_count;
        }
    }

    pb_create_mapping.finish();
}

void TrigramBuilder::create_shards(MemoryMappedFile<uint8_t> &input_file, std::vector<uint64_t> &line_boundaries, uint64_t longest_line, ShardsManager<ShardIndexPair> &shards_manager)
{

    uint64_t num_lines = line_boundaries.size();

    Progress pb_create_shards("creating shards", num_lines);

    auto shard_worker = [&](uint64_t start, uint64_t end, uint64_t step)
    {
        std::vector<uint32_t> line_grams(longest_line - 2);

        auto shards = shards_manager.create_shard_buffers();

        for (uint32_t i = start; i < end; i += step)
        {
            if (i % pb_create_shards.quantum == 0)
                pb_create_shards.add(pb_create_shards.quantum);

            uint8_t *start = input_file + line_boundaries.at(i) + 1,
                    *end = input_file + line_boundaries.at(i + 1);

            TrigramBuilder::generate_unique_trigrams(line_grams, start, end);

            for (auto gram : line_grams)
            {
                uint8_t shard_index = gram >> 16;
                uint16_t shard_offset = gram & SHARD_OFFSET_MASK;
                shards[shard_index].push(ShardIndexPair{shard_offset, i});
            }
        }

        for (auto &shard : shards)
            shard.flush();
    };

    ThreadingPool creation_pool(num_lines - 1, shard_worker);
    creation_pool.join();

    pb_create_shards.finish();
}

void TrigramBuilder::process_shards(ShardsManager<ShardIndexPair> &shards_manager, MemoryMappedFile<MappingIndex> &mapping)
{
    int num_non_empty_shards = shards_manager.find_non_empty_shards();

    std::cout << "found " << num_non_empty_shards << " nonempty shards" << std::endl;

    Progress pb_process_shards("processing shards", num_non_empty_shards);

    auto index_file = ::fopen("index.bin.new", "wb");

    std::condition_variable wait_condition;
    std::mutex mutex;
    using guard = std::unique_lock<std::mutex>;
    uint64_t current_job = 0;

    //auto shard_worker = [&](uint64_t start, uint64_t end, uint64_t step)
    //{
        for (int i = 0; i < num_non_empty_shards; i += 1)
        {
            int shard_id = shards_manager.non_empty_shards[i];

            auto consolidated = shards_manager.shards[shard_id].consolidate();

            auto consolidated_start = consolidated->begin();
            auto consolidated_end = consolidated->end();


            std::sort(
                consolidated_start,
                consolidated_end,
                [](const ShardIndexPair &a, const ShardIndexPair &b)
                {
                    if (a.shard_offset != b.shard_offset)
                        return a.shard_offset < b.shard_offset;
                    return a.line_id < b.line_id;
                });

            uint32_t last_shard_offset = -1;
            uint64_t current_count = 0;

            for (ShardIndexPair *pair = consolidated_start; pair < consolidated_end; pair++)
            {
                if (last_shard_offset == -1)
                    last_shard_offset = pair->shard_offset;

                if (last_shard_offset != pair->shard_offset)
                {
                    mapping[(shard_id << 16) | last_shard_offset].count = current_count;
                    current_count = 0;
                    last_shard_offset = pair->shard_offset;
                }

                current_count++;
            }

            if (current_count)
                mapping[(shard_id << 16) | last_shard_offset].count = current_count;

            guard lock(mutex);
            wait_condition.wait(lock, [&]{ return i == current_job; });

            for (ShardIndexPair *pair = consolidated_start; pair < consolidated_end; pair++)
                ::fwrite(&pair->line_id, sizeof(uint32_t), 1, index_file);

            pb_process_shards.add();

            current_job++;

            wait_condition.notify_all();
        }
    //};

    // ThreadingPool processing_pool(num_non_empty_shards, shard_worker);
    //processing_pool.join();

    pb_process_shards.finish();

    std::fclose(index_file);
}

void TrigramBuilder::create_offsets_file(std::vector<uint64_t> &line_boundaries)
{
    MemoryMappedFile<uint64_t> offsets("offsets.bin.new", line_boundaries.size());

    for (size_t i = 1; i < line_boundaries.size(); i++)
        offsets[i] = line_boundaries[i];

    offsets[0] = 0;
}

uint64_t TrigramBuilder::compute_line_boundaries(MemoryMappedFile<uint8_t> &input_file, std::vector<uint64_t> &line_boundaries)
{
    line_boundaries.push_back(-1);

    Progress pb_compute_lines("find line boundaries", input_file.size());

    for (size_t i = 0; i < input_file.size(); i++)
    {
        if (i % 1000 == 0)
            pb_compute_lines.update(i);

        if (input_file[i] == '\n')
            line_boundaries.push_back(i);
    }

    line_boundaries.push_back(input_file.size());

    std::cout << "\nFound " << line_boundaries.size() - 1 << " lines" << std::endl;

    return line_boundaries.size();
}

size_t TrigramBuilder::get_longest_line(std::vector<uint64_t> &line_boundaries)
{
    size_t longest_line = 0;

    for (size_t i = 0; i + 1 < line_boundaries.size(); i++)
        longest_line = std::max(longest_line, line_boundaries[i + 1] - line_boundaries[i]) - 1;

    std::cout << "Longest line: " << longest_line << " characters" << std::endl;

    return longest_line;
}

void TrigramBuilder::generate_unique_trigrams(std::vector<uint32_t> &grams, uint8_t *buffer, uint8_t *end)
{
    grams.clear();

    for (; buffer + 2 < end; buffer++)
    {
        uint32_t trigram = TrigramBuilder::build_trigram(buffer);
        grams.push_back(trigram);
    }

    std::sort(grams.begin(), grams.end());

    grams.erase(std::unique(grams.begin(), grams.end()), grams.end());
}

uint32_t TrigramBuilder::build_trigram(uint8_t *buffer)
{
    return (buffer[2] << 16) + (buffer[1] << 8) + buffer[0];
}

void TrigramBuilder::replace_active_index(MemoryMappedFile<uint8_t> &input_file) {
    if (::rename("mapping.bin.new", "mapping.bin") != 0) {
        std::perror("rename");
        std::runtime_error("failed moving mapping.bin.new to mapping.bin");
    }

    if (::rename("index.bin.new", "index.bin") != 0) {
        std::perror("rename");
        std::runtime_error("failed moving mapping.bin.new to mapping.bin");
    }

    if (::rename("offsets.bin.new", "offsets.bin") != 0) {
        std::perror("rename");
        std::runtime_error("failed moving mapping.bin.new to mapping.bin");
    }

    if (::rename("ids.bin.new", "ids.bin") != 0) {
        std::perror("rename");
        std::runtime_error("failed moving mapping.bin.new to mapping.bin");
    }

    if (::rename("content.bin.new", "content.bin") != 0) {
        std::perror("rename");
        std::runtime_error("failed moving mapping.bin.new to mapping.bin");
    }
}