#ifndef QUASARDB_RENDERER_H
#define QUASARDB_RENDERER_H

#include <string>

namespace qdb::client {

    class Renderer {
    public:
        void RenderWelcome() const;

        void RenderPrompt() const;

        void RenderResult(const std::string &json_data) const;

        void RenderError(const std::string &message) const;
    };

} // namespace qdb::client

#endif  // QUASARDB_RENDERER_H

