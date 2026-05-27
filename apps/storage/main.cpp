#include <qdb/storage/application.h>

#include <iostream>

int main(int argc, char** argv) {
    try {
        auto config = qdb::storage::Config::FromArguments(argc, argv);

        if (!config.has_value()) {
            return 0;
        }

        auto application = qdb::storage::Application::New(config.value());

        return application->Run();
    } catch (const std::exception& exception) {
        std::cerr << "[FATAL]: " << exception.what() << std::endl;

        return 1;
    }
}
