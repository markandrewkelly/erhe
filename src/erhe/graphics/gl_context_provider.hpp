#pragma once

#include "concurrentqueue.h"

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace erhe::toolkit
{
    class Context_window;
}

namespace erhe::graphics {

class Instance;
class OpenGL_state_tracker;

class Gl_worker_context
{
public:
    int                            id{0};
    erhe::toolkit::Context_window* context{nullptr};
};

class Gl_context_provider
{
public:
    Gl_context_provider(
        Instance&             graphics_instance,
        OpenGL_state_tracker& opengl_state_tracker
    );

    Gl_context_provider (const Gl_context_provider&) = delete;
    void operator=      (const Gl_context_provider&) = delete;
    Gl_context_provider (Gl_context_provider&&)      = delete;
    void operator=      (Gl_context_provider&&)      = delete;

    // Public API
    [[nodiscard]] auto acquire_gl_context() -> Gl_worker_context;
    void release_gl_context     (Gl_worker_context context);
    void provide_worker_contexts(
        erhe::toolkit::Context_window* main_window,
        std::function<bool()>          worker_contexts_still_needed_callback
    );

private:
    erhe::graphics::Instance&                                   m_graphics_instance;
    OpenGL_state_tracker&                                       m_opengl_state_tracker;

    erhe::toolkit::Context_window*                              m_main_window{nullptr};
    std::thread::id                                             m_main_thread_id;
    std::mutex                                                  m_mutex;
    std::condition_variable                                     m_condition_variable;
    moodycamel::ConcurrentQueue<Gl_worker_context>              m_worker_context_pool;
    std::vector<std::shared_ptr<erhe::toolkit::Context_window>> m_contexts;
};

class Scoped_gl_context
{
public:
    explicit Scoped_gl_context(Gl_context_provider& context_provider);
    ~Scoped_gl_context() noexcept;
    Scoped_gl_context (const Scoped_gl_context&) = delete;
    auto operator=    (const Scoped_gl_context&) = delete;
    Scoped_gl_context (Scoped_gl_context&&)      = delete;
    auto operator=    (Scoped_gl_context&&)      = delete;

private:
    Gl_context_provider& m_context_provider;
    Gl_worker_context    m_context;
};

} // namespace erhe::graphics
