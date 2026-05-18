#include <qdb/storage/application.h>


int main(int argc, char** argv) {
    auto config = qdb::storage::Config::FromArguments(argc, argv);

    auto application = qdb::storage::Application::New(config);

    auto result = application->Run();

    return result;
}
