#ifndef QUASARDB_RENDERER_H
#define QUASARDB_RENDERER_H

#include <qdb/core/response.h>

#include <string>

namespace qdb::client {

class Renderer {
public:
    void RenderWelcome() const;

    void RenderResponse(const qdb::core::Response& response) const;

    void RenderError(const std::string& message) const;

    void RenderAsyncSubmitted(const std::string& task_id) const;

    void RenderPollingProgress(char spinner_char, std::size_t attempt) const;

private:
    void RenderTable(const std::string& json_data) const;
};

}  // namespace qdb::client

#endif  // QUASARDB_RENDERER_H
