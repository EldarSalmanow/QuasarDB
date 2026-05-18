#include <qdb/server/application.h>


int main(int argc, char** argv) {
    auto config = qdb::server::Config::New();

    auto application = qdb::server::Application::New(config);

    auto result = application->Run();

    return result;
}
