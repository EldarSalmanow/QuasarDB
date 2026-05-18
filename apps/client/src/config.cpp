#include <qdb/client/config.h>

#include <args.hxx>

#include <iostream>
#include <stdexcept>


namespace qdb::client {

Config::Config(std::string host, std::uint32_t port, std::string file)
    : host_(std::move(host)), port_(port), file_(std::move(file)) {}

auto Config::New(std::string host, std::uint32_t port, std::string file) -> Config {
    return Config(std::move(host), port, std::move(file));
}

auto Config::FromArguments(int argc, char** argv) -> std::optional<Config> {
    args::ArgumentParser parser("QuasarDB client");
    args::HelpFlag help(parser, "help", "Display this help", {'?', "help"});
    args::ValueFlag<std::string> host(parser, "host", "Server host (default: localhost)", {'H', "host"}, "localhost");
    args::ValueFlag<int> port(parser, "port", "Server port (default: 4567)", {'p', "port"}, 4567);
    args::ValueFlag<std::string> file(parser, "file", "Execute commands from file", {'f', "file"}, "");

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help &) {
        std::cout << parser;

        return std::nullopt;
    } catch (const args::ParseError &exception) {
        throw std::runtime_error(std::string("Argument parse error: ") + exception.what());
    }

    if (port.Get() <= 0) {
        throw std::runtime_error("Invalid port number");
    }

    return Config(host.Get(), static_cast<std::uint32_t>(port.Get()), file.Get());
}

auto Config::Host() const -> const std::string& {
    return host_;
}

auto Config::Port() const -> std::uint32_t {
    return port_;
}

auto Config::File() const -> const std::string& {
    return file_;
}

}  // namespace qdb::client

