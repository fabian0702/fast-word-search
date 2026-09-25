#include "database.hpp"

const char *get_connection_url() {
    char *connection_url = std::getenv("POSTGRES_CONNECTION_URL");

    std::cout << "conn url: " << connection_url << std::endl;

    if (connection_url != nullptr)
        return connection_url;
    else {
        std::cout << "no database url specified, trying to connect to localhost" << std::endl;
        
        return "postgresql://search:password@localhost/search";
    }
}

DatabaseConnector::DatabaseConnector() : conn(get_connection_url()) {}

void DatabaseConnector::request_content()
{
    pqxx::work transaction(conn);

    auto results_size_result = transaction.exec("SELECT COUNT(*) AS words, SUM(octet_length(word)) AS bytes FROM words").one_row();

    if (results_size_result["bytes"].is_null() | results_size_result["words"].is_null())
    {
        throw std::runtime_error("failed to run size query");
    }

    std::size_t bytes = results_size_result["bytes"].as<std::size_t>();
    std::size_t words = results_size_result["words"].as<std::size_t>();

    size_t total_content_size = bytes + words; // compensate for '\n' by adding the number of words

    std::cout << "Total words: " << words << " bytes: " << bytes << std::endl;

    MemoryMappedFile<char> content_file("content.bin.new", total_content_size);
    MemoryMappedFile<uint64_t> ids_file("ids.bin.new", words);

    uint64_t word_file_offset = 0;
    uint64_t ids_file_offset = 0;

    auto result = transaction.exec(
        "SELECT id, word FROM words ORDER BY id");

    for (const auto &row : result)
    {
        std::string word = row["word"].as<std::string>();

        for (const char c : word)
            content_file.at(word_file_offset++) = c;

        content_file.at(word_file_offset++) = '\n';

        size_t word_id = row["id"].as<uint64_t>();
        ids_file[ids_file_offset++] = word_id;
    }
}