#include <qdb/storage/config.h>

#include <args.hxx>

namespace qdb::storage {

Config::Config(std::string host, std::uint32_t port, std::string root)
    : host_(std::move(host)), port_(port), root_(std::move(root)) {}

Config Config::New(std::string host, std::uint32_t port, std::string root) {
    return Config{std::move(host), port, std::move(root)};
}

auto Config::FromArguments(int argc, char** argv) -> std::optional<Config> {
    args::ArgumentParser parser("QuasarDB storage");
    args::HelpFlag help(parser, "help", "Display this help", {'?', "help"});
    args::ValueFlag<std::string> host(parser, "host", "Bind host (default: 127.0.0.1)", {'H', "host"}, "127.0.0.1");
    args::ValueFlag<int> port(parser, "port", "Bind port (default: 7000)", {'p', "port"}, 7000);
    args::ValueFlag<std::string> root(parser, "root", "Storage shard root", {"root"}, "storage");

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << parser;

        return std::nullopt;
    } catch (const args::ParseError& exception) {
        throw std::runtime_error(std::string("Argument parse error: ") + exception.what());
    }

    if (port.Get() <= 0) {
        throw std::runtime_error("Invalid port number");
    }

    return Config::New(host.Get(), static_cast<std::uint32_t>(port.Get()), root.Get());
}

auto Config::Host() const -> const std::string& { return host_; }

auto Config::Port() const -> std::uint32_t { return port_; }

auto Config::Root() const -> const std::string& { return root_; }

}  // namespace qdb::storage
