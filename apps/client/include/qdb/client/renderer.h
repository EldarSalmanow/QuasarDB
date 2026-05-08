#ifndef QUASARDB_RENDERER_H
#define QUASARDB_RENDERER_H

#include <string>

namespace qdb::client {

class Response;

class Renderer {
public:
    void RenderWelcome() const;

    void RenderPrompt() const;

    void RenderResponse(const Response& response) const;

    void RenderError(const std::string& message) const;

private:
    void RenderTable(const std::string& json_data) const;
};

}  // namespace qdb::client

#endif  // QUASARDB_RENDERER_H
