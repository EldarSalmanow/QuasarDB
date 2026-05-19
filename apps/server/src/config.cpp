#include <qdb/server/config.h>

#include <args.hxx>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace qdb::server {

Config::Config(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
               std::string account_path, std::string rbac_path, std::string access_log_path,
               std::vector<StorageNode> storage_nodes)
    : host_(std::move(host)),
      port_(port),
      auth_required_(auth_required),
      jwt_secret_(std::move(jwt_secret)),
      account_path_(std::move(account_path)),
      rbac_path_(std::move(rbac_path)),
      access_log_path_(std::move(access_log_path)),
      storage_nodes_(std::move(storage_nodes)) {}

auto Config::New(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
                 std::string account_path, std::string rbac_path, std::string access_log_path,
                 std::vector<StorageNode> storage_nodes) -> Config {
    return Config(std::move(host), port, auth_required, std::move(jwt_secret), std::move(account_path),
                  std::move(rbac_path), std::move(access_log_path), std::move(storage_nodes));
}

auto Config::FromArguments(int argc, char** argv) -> std::optional<Config> {
    args::ArgumentParser parser("QuasarDB server");
    args::HelpFlag help(parser, "help", "Display this help", {'?', "help"});
    args::ValueFlag<std::string> host(parser, "host", "Bind host (default: 127.0.0.1)", {'H', "host"}, "127.0.0.1");
    args::ValueFlag<int> port(parser, "port", "Bind port (default: 9000)", {'p', "port"}, 9000);
    args::Flag auth(parser, "auth", "Require JWT authentication", {'a', "auth"});
    args::ValueFlag<std::string> secret(parser, "secret", "JWT secret", {'s', "secret"}, "quasardb-dev-secret");
    args::ValueFlag<std::string> accounts(parser, "accounts", "Accounts file", {'A', "accounts"}, "qdb_accounts.json");
    args::ValueFlag<std::string> rbac(parser, "rbac", "RBAC rules file", {'R', "rbac"}, "qdb_rbac.json");
    args::ValueFlag<std::string> log(parser, "log", "Access log file", {'L', "log"}, "qdb_access.log");

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

    return Config::New(host.Get(), static_cast<std::uint32_t>(port.Get()), auth.Get(), secret.Get(), accounts.Get(),
                       rbac.Get(), log.Get());
}

auto Config::Host() const -> const std::string& {
    return host_;
}

auto Config::Port() const -> std::uint32_t {
    return port_;
}

auto Config::AuthRequired() const -> bool {
    return auth_required_;
}

auto Config::JwtSecret() const -> const std::string& {
    return jwt_secret_;
}

auto Config::AccountPath() const -> const std::string& {
    return account_path_;
}

auto Config::RbacPath() const -> const std::string& {
    return rbac_path_;
}

auto Config::AccessLogPath() const -> const std::string& {
    return access_log_path_;
}

auto Config::StorageNodes() const -> const std::vector<StorageNode>& {
    return storage_nodes_;
}

}  // namespace qdb::server
