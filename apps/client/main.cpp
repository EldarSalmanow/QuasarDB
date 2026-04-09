#include <qdb/client/application.h>

/*
 * TODO: add signal handlers
 */

int main(int argc, char **argv) {
    auto config = qdb::client::Config::Load("<path>");

    if (!config) {
        return 1;
    }

    const auto application = qdb::client::Application::New(std::move(config));

    const auto result = application->Run();

    return result;
}
