#include <sdk.h>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include "json_loader.h"
#include "request_handler.h"

namespace net = boost::asio;

namespace {
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> v;
    v.reserve(n - 1);
    for (unsigned i = 0; i < n - 1; ++i) { v.emplace_back(fn); }
    fn();
    for (auto& t : v) { t.join(); }
}
}

int main(int argc, const char* argv[]) {
    // В Спринте 2 тесты передают 3 аргумента: exe, config, static_dir
    if (argc < 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-pure-dir>" << std::endl;
        return EXIT_FAILURE;
    }
    try {
        model::Game game = json_loader::LoadGame(argv[1]);
        std::filesystem::path static_path{argv[2]};
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        // Передаем static_path в RequestHandler
        auto handler = std::make_shared<http_handler::RequestHandler>(game, static_path);
        
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // КРИТИЧЕСКИ ВАЖНО: Тесты ждут именно эту фразу
        std::cout << "Server has started..." << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
