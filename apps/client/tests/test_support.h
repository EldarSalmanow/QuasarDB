#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace qdb::client::test {

class OutputCapture {
public:
    explicit OutputCapture(std::ostream& stream) : stream_(stream), old_(stream.rdbuf(buffer_.rdbuf())) {}
    ~OutputCapture() { stream_.rdbuf(old_); }

    OutputCapture(const OutputCapture&) = delete;
    OutputCapture& operator=(const OutputCapture&) = delete;

    auto Str() const -> std::string { return buffer_.str(); }

private:
    std::ostream& stream_;
    std::ostringstream buffer_;
    std::streambuf* old_;
};

class InputRedirect {
public:
    explicit InputRedirect(std::string data) : input_(std::move(data)), old_(std::cin.rdbuf(input_.rdbuf())) {}
    ~InputRedirect() { std::cin.rdbuf(old_); }

    InputRedirect(const InputRedirect&) = delete;
    InputRedirect& operator=(const InputRedirect&) = delete;

private:
    std::istringstream input_;
    std::streambuf* old_;
};

class ScopedTempFile {
public:
    explicit ScopedTempFile(std::string content, std::string prefix = "qdb_client_test", std::string suffix = ".sql")
        : path_(MakeUniquePath(std::move(prefix), std::move(suffix))) {
        std::ofstream out(path_);
        if (!out.is_open()) {
            throw std::runtime_error("Failed to create temp file");
        }
        out << std::move(content);
    }

    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    ScopedTempFile(const ScopedTempFile&) = delete;
    ScopedTempFile& operator=(const ScopedTempFile&) = delete;

    auto Path() const -> const std::filesystem::path& { return path_; }

private:
    static auto MakeUniquePath(std::string prefix, std::string suffix) -> std::filesystem::path {
        static std::atomic<std::uint64_t> counter{0};
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() /
               (std::move(prefix) + "_" + std::to_string(stamp) + "_" + std::to_string(++counter) + std::move(suffix));
    }

    std::filesystem::path path_;
};

inline auto FindFreePort() -> std::uint32_t {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return 0;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        ::close(fd);
        return 0;
    }

    ::close(fd);
    return ntohs(addr.sin_port);
}

inline auto Contains(const std::string& haystack, const std::string& needle) -> bool {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace qdb::client::test
