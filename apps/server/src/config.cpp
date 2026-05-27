#include <qdb/server/config.h>

#include <args.hxx>

#include <iostream>
#include <stdexcept>
#include <utility>


namespace qdb::server {

Config::Config(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
               std::string account_path, std::string rbac_path, bool auto_start_storage,
               std::string storage_binary, std::string storage_root)
    : host_(std::move(host)),
      port_(port),
      auth_required_(auth_required),
      jwt_secret_(std::move(jwt_secret)),
      account_path_(std::move(account_path)),
      rbac_path_(std::move(rbac_path)),
      auto_start_storage_(auto_start_storage),
      storage_binary_(std::move(storage_binary)),
      storage_root_(std::move(storage_root)) {}

auto Config::New(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
                 std::string account_path, std::string rbac_path, bool auto_start_storage,
                 std::string storage_binary, std::string storage_root) -> Config {
    return Config(std::move(host), port, auth_required, std::move(jwt_secret), std::move(account_path),
                  std::move(rbac_path), auto_start_storage, std::move(storage_binary), std::move(storage_root));
}

auto Config::FromArguments(int argc, char** argv) -> std::optional<Config> {
    args::ArgumentParser parser("QuasarDB server");
    args::HelpFlag help(parser, "help", "Display this help", {'?', "help"});
    args::ValueFlag<std::string> host(parser, "host", "Bind host (default: 127.0.0.1)", {'H', "host"}, "127.0.0.1");
    args::ValueFlag<int> port(parser, "port", "Bind port (default: 9000)", {'p', "port"}, 9000);
    args::Flag no_auth(parser, "no-auth", "Disable JWT authentication", {"no-auth"});
    args::ValueFlag<std::string> secret(parser, "secret", "JWT secret", {'s', "secret"}, "quasardb-dev-secret");
    args::ValueFlag<std::string> accounts(parser, "accounts", "Accounts file", {'A', "accounts"}, "qdb_accounts.json");
    args::ValueFlag<std::string> rbac(parser, "rbac", "RBAC rules file", {'R', "rbac"}, "qdb_rbac.json");
    args::Flag no_storage(parser, "no-storage-autostart", "Do not launch qdb-storage processes", {"no-storage-autostart"});
    args::ValueFlag<std::string> storage_bin(parser, "storage-bin", "qdb-storage executable", {"storage-bin"}, "");
    args::ValueFlag<std::string> storage_root(parser, "storage-root", "Storage shards root", {"storage-root"}, "data");

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

    return Config::New(host.Get(), static_cast<std::uint32_t>(port.Get()), !no_auth.Get(), secret.Get(), accounts.Get(),
                       rbac.Get(), !no_storage.Get(), storage_bin.Get(), storage_root.Get());
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

auto Config::AutoStartStorage() const -> bool {
    return auto_start_storage_;
}

auto Config::StorageBinary() const -> const std::string& {
    return storage_binary_;
}

auto Config::StorageRoot() const -> const std::string& {
    return storage_root_;
}

}  // namespace qdb::server
