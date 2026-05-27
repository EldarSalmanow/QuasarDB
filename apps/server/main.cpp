#include <qdb/server/application.h>

#include <iostream>

int main(int argc, char** argv) {
    try {
        auto config = qdb::server::Config::FromArguments(argc, argv);

        if (!config.has_value()) {
            return 0;
        }

        auto application = qdb::server::Application::New(config.value());

        return application->Run();
    } catch (const std::exception& exception) {
        std::cerr << "[FATAL]: " << exception.what() << std::endl;

        return 1;
    }
}
