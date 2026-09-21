#include "trigram_builder.hpp"
#include "trigram_query.hpp"

#include "httplib.h"

#include <mutex>

int main(int argc, char *argv[])
{
    pid_t pid = getpid();
    std::cout << "Process ID: " << pid << std::endl;

    httplib::Server svr;

    std::mutex build_mutex;

    svr.Get("/build", [&build_mutex](const httplib::Request &req, httplib::Response &res)
            {
        std::lock_guard build_in_progress(build_mutex);         // enforce that only one build process runs at the same time (they might overwrite each others shards / output files)

        bool build_ids = req.has_param("ids") && req.get_param_value("ids") == "linear";

        TrigramBuilder::build(build_ids);

        res.status = 200;
        res.set_content("index rebuilt", "text/plain"); 
    });

    svr.Get("/search", [](const httplib::Request &req, httplib::Response &res)
            {

        if (!req.has_param("query")) {
            res.status = 400;
            res.set_content("No query parameter specified", "text/plain");

            return;
        }

        std::string query = req.get_param_value("query");
        
        TrigramQuery query_engine;

        auto results = query_engine.query(query);

        res.status = 200;
        res.set_content((const char *)results.data(), results.size() * sizeof(uint64_t), "application/octet-stream");
    });

    svr.set_exception_handler([](const auto &req, auto &res, std::exception_ptr ep)
                              {
        auto fmt = "<h1>Error 500</h1><p>%s</p>";
        char buf[BUFSIZ];
        try {
            std::rethrow_exception(ep);
        } catch (std::exception &e) {
            snprintf(buf, sizeof(buf), fmt, e.what());
        } catch (...) { // See the following NOTE
            snprintf(buf, sizeof(buf), fmt, "Unknown Exception");
        }
        res.set_content(buf, "text/html");
        res.status = 500; 
    });

    svr.listen("0.0.0.0", 8000);
}