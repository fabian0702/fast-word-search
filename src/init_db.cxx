#include <iostream>
#include <fstream> 

#include <pqxx/pqxx>

#include "progress.hpp"
#include "database.hpp"
#include "memory_mapped_file.hpp"
#include "trigram_builder.hpp"

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: program <words file>" << std::endl;
        std::exit(1);
    }

    std::string word;
    std::ifstream words_file(argv[1]);

    std::cout << "counting lines..." << std::endl;

    MemoryMappedFile<uint8_t> input_file(argv[1]);
    
    std::vector<uint64_t> line_boundaries;
    auto num_words = TrigramBuilder::compute_line_boundaries(input_file, line_boundaries);

    // auto num_words = std::count_if(std::istreambuf_iterator<char>{words_file}, {}, [](char c) { return c == '\n'; });

    pqxx::connection conn(get_connection_url());

    pqxx::work transaction(conn);

    auto stream = pqxx::stream_to::raw_table(transaction, "words", "word");

    Progress pb_insert_words("Inserting words", num_words);
    
    words_file.seekg(0);

    while (getline(words_file, word)) {
        stream.write_values(word);

        pb_insert_words.add(1);
    }

    pb_insert_words.finish();

    stream.complete();

    transaction.commit();
}