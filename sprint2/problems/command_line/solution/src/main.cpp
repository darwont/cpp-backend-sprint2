#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

namespace net = boost::asio;
namespace sys = boost::system;

// Структура для хранения разобранных параметров
struct Args {
    int tick_period = 0;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

// Функция парсинга командной строки
[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    po::options_description desc{"Allowed options"};
    Args args;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }
    if (!vm.contains("config-file")) {
        throw std::runtime_error("Config file path is not specified");
    }
    if (!vm.contains("www-root")) {
        throw std::runtime_error("Static files root is not specified");
    }

    return args;
}

int main(int argc, const char* argv[]) {
    try {
        // Парсим аргументы вместо жесткой проверки argc != 2
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS; // Завершаем, если запросили --help
        }

        // Загружаем игру, используя путь из командной строки
        model::Game game = json_loader::LoadGame(args->config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int) {
            if (!ec) ioc.stop();
        });

        // ПРИМЕЧАНИЕ: Сейчас твой RequestHandler принимает только game.
        // Чтобы сервер отдавал статические файлы, в будущем нужно будет передать 
        // в него путь к статике: args->www_root
        auto handler = std::make_shared<http_handler::RequestHandler>(game, args->www_root);
        
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..." << std::endl;

        std::vector<std::thread> workers;
        for (unsigned i = 0; i < num_threads - 1; ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }
        ioc.run();
        for (auto& w : workers) { w.join(); }
        
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}