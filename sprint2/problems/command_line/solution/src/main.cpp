#include "http_server.h"
#include "request_handler.h"
#include "logging_handler.h"
#include "json_loader.h"
#include "ticker.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <thread>

using namespace std::literals;
namespace net = boost::asio;
namespace po = boost::program_options;
namespace logging = boost::log;
namespace json = boost::json;

namespace server_logging {
    boost::log::attributes::keyword<boost::json::value> additional_data("AdditionalData");
}
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<int> tick_period;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"Allowed options"s};
    Args args;
    int tick_period_raw = 0;

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&tick_period_raw)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }
    if (!vm.contains("config-file"s)) throw std::runtime_error("Config file path is not specified"s);
    if (!vm.contains("www-root"s)) throw std::runtime_error("Static files root is not specified"s);
    if (vm.contains("tick-period"s)) args.tick_period = tick_period_raw;
    
    return args;
}

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_obj;
    log_obj["timestamp"] = boost::posix_time::to_iso_extended_string(*rec[timestamp]);
    if (rec.count(server_logging::additional_data)) log_obj["data"] = rec[server_logging::additional_data].get();
    else log_obj["data"] = json::object();
    log_obj["message"] = *rec[logging::expressions::smessage];
    strm << json::serialize(log_obj);
}

void InitLogger() {
    logging::add_common_attributes();
    logging::add_console_log(std::cout, boost::log::keywords::format = &JsonFormatter, boost::log::keywords::auto_flush = true);
}

void RunWorkers(unsigned num_threads, const std::function<void()>& fn) {
    num_threads = std::max(1u, num_threads);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < num_threads - 1; ++i) workers.emplace_back(fn);
    fn();
    for (auto& t : workers) t.join();
}

int main(int argc, const char* argv[]) {
    InitLogger();
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) return EXIT_SUCCESS;

        model::Game game = json_loader::LoadGame(args->config_file);
        app::Application app(std::move(game));

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(args->www_root, api_strand, app);

        server_logging::LoggingRequestHandler logging_handler{
            [handler](auto&& endpoint, auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(endpoint)>(endpoint), std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            }
        };

        std::shared_ptr<time_control::Ticker> ticker;
        if (args->tick_period) {
            ticker = std::make_shared<time_control::Ticker>(api_strand, std::chrono::milliseconds(*args->tick_period),
                [&app](std::chrono::milliseconds delta) { app.Tick(delta); }
            );
            ticker->Start();
        }

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) { ioc.stop(); });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        json::value start_data{{"port", port}, {"address", address.to_string()}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(server_logging::additional_data, start_data) << "server started"sv;

        RunWorkers(num_threads, [&ioc] { ioc.run(); });

        json::value exit_data{{"code", 0}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(server_logging::additional_data, exit_data) << "server exited"sv;
        return 0;
    } catch (const std::exception& e) {
        json::value error_data{{"code", EXIT_FAILURE}, {"exception", e.what()}};
        BOOST_LOG_TRIVIAL(fatal) << logging::add_value(server_logging::additional_data, error_data) << "server exited"sv;
        return EXIT_FAILURE;
    }
}
