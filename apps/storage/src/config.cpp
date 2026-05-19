#include <qdb/storage/config.h>


namespace qdb::storage {

Config::Config(std::string host, std::uint32_t port, std::string root)
    : host_(std::move(host)),
      port_(port),
      root_(std::move(root)) {}

Config Config::New(std::string host, std::uint32_t port, std::string root) {
    return Config {
        std::move(host), port, std::move(root)
    };
}

auto Config::FromArguments(int argc, char **argv) -> Config {
    // TODO: refactor to using taywee-args

    std::string host = "127.0.0.1";
    std::uint32_t port = 7000;
    std::string data_root = "data/storage";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<std::uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--data-dir" && i + 1 < argc) {
            data_root = argv[++i];
        }
    }

    return Config(std::move(host), port, std::move(data_root));
}

auto Config::Host() const -> const std::string & {
    return host_;
}

auto Config::Port() const -> std::uint32_t {
    return port_;
}

auto Config::Root() const -> const std::string & {
    return root_;
}

}
