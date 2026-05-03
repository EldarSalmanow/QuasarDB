#include <qdb/client/application.h>

#include <iostream>

/*
 * TODO: add signal handlers
 */

int main(int argc, char** argv) {
    (void)argc, (void)argv;

    std::cout << "QuasarDB Client" << std::endl;

    return 0;
    // auto config = qdb::client::Config::Load("<path>");
    //
    // if (!config) {
    //     return 1;
    // }
    //
    // const auto application = qdb::client::Application::New(config);
    //
    // const auto result = application->Run();
    //
    // return result;
}
