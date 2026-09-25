#pragma once

#include <iostream>

#include <pqxx/transaction>
#include <pqxx/pqxx>

#include "memory_mapped_file.hpp"

class DatabaseConnector {
    pqxx::connection conn;

public:
    DatabaseConnector();

    void request_content();
};