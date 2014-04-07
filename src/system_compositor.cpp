/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 */

#include "dbus_screen.h"
#include "system_compositor.h"

#include <mir/run_mir.h>
#include <mir/abnormal_exit.h>
#include <mir/default_server_configuration.h>
#include <mir/options/default_configuration.h>
#include <mir/frontend/shell.h>
#include <mir/scene/surface.h>
#include <mir/scene/surface_ranker.h>
#include <mir/server_status_listener.h>
#include <mir/shell/session.h>
#include <mir/shell/focus_controller.h>
#include <mir/input/cursor_listener.h>

#include <cerrno>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <regex.h>
#include <GLES2/gl2.h>
#include <boost/algorithm/string.hpp>
#include <QCoreApplication>

namespace geom = mir::geometry;
namespace msh = mir::shell;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace mo = mir::options;
namespace ms = mir::scene;
namespace po = boost::program_options;

class SystemCompositorSurface;

class SystemCompositorSession : public msh::Session
{
public:
    SystemCompositorSession(std::shared_ptr<msh::Session> const& self,
                            SystemCompositorShell *shell)
        : self{self}, shell{shell}, ready{false} {}

    // These are defined below, since they reference methods defined in other classes
    void mark_ready();
    void raise(std::shared_ptr<ms::SurfaceRanker> const& controller);
    std::shared_ptr<mf::Surface> get_surface(mf::SurfaceId surface) const;
    mf::SurfaceId create_surface(msh::SurfaceCreationParameters const& params);

    bool is_ready() const
    {
        return ready;
    }

    std::shared_ptr<msh::Session> get_orig()
    {
        return self;
    }

    void destroy_surface(mf::SurfaceId surface)
    {
        surfaces.erase(surface);
        self->destroy_surface(surface);
    }

    std::string name() const {return self->name();}
    void hide() {self->hide();}
    void show() {self->show();}
    void send_display_config(mg::DisplayConfiguration const&config) {self->send_display_config(config);}
    int configure_surface(mf::SurfaceId id, MirSurfaceAttrib attrib, int value) {return self->configure_surface(id, attrib, value);}

    // msh::Session methods
    pid_t process_id() const {return self->process_id();}
    void force_requests_to_complete() {self->force_requests_to_complete();}
    void take_snapshot(msh::SnapshotCallback const& snapshot_taken) {self->take_snapshot(snapshot_taken);}
    std::shared_ptr<msh::Surface> default_surface() const {return self->default_surface();}
    void set_lifecycle_state(MirLifecycleState state) {self->set_lifecycle_state(state);}

private:
    std::shared_ptr<msh::Session> const self;
    SystemCompositorShell *shell;
    std::map<mf::SurfaceId, std::shared_ptr<SystemCompositorSurface>> surfaces;
    bool ready;
};

class SystemCompositorSurface : public ms::Surface
{
public:
    SystemCompositorSurface(std::shared_ptr<ms::Surface> const& self,
                            SystemCompositorSession *session)
        : self{self}, session{session}, buffer_count{0} {}

    std::shared_ptr<ms::Surface> get_orig()
    {
        return self;
    }

    void swap_buffers(mg::Buffer* old_buffer, std::function<void(mg::Buffer* new_buffer)> complete)
    {
        self->swap_buffers(old_buffer, complete);
        // If we have content (and we're not the first buffer -- first buffer
        // is actually not enough in my experience; maybe a bug in Qt?)
        if (old_buffer != NULL && !session->is_ready() && buffer_count++ == 2)
            session->mark_ready();
    }

    // mf::Surface methods
    void force_requests_to_complete() {self->force_requests_to_complete();}
    geom::Size size() const {return self->size();}
    MirPixelFormat pixel_format() const {return self->pixel_format();}
    bool supports_input() const {return self->supports_input();}
    int client_input_fd() const {return self->client_input_fd();}
    int configure(MirSurfaceAttrib attrib, int value) {return self->configure(attrib, value);}

    // msh::Surface methods
    std::string name() const {return self->name();}
    MirSurfaceType type() const {return self->type();}
    MirSurfaceState state() const {return self->state();}
    void hide() {self->hide();}
    void show() {self->show();}
    void move_to(geom::Point const& top_left) {self->move_to(top_left);}
    geom::Point top_left() const {return self->top_left();}
    void take_input_focus(std::shared_ptr<msh::InputTargeter> const& targeter) {self->take_input_focus(targeter);}
    void set_input_region(std::vector<geom::Rectangle> const& region) {self->set_input_region(region);}
    void allow_framedropping(bool allow) {self->allow_framedropping(allow);}
    void resize(geom::Size const& size) {self->resize(size);}
    void set_transformation(glm::mat4 const& t) {self->set_transformation(t);}
    float alpha() const {return self->alpha();}
    void set_alpha(float alpha) {self->set_alpha(alpha);}
    void with_most_recent_buffer_do(std::function<void(mg::Buffer&)> const& exec) {self->with_most_recent_buffer_do(exec);}

    // ms::Surface methods
    std::shared_ptr<mi::InputChannel> input_channel() const {return self->input_channel();}
    void on_change(std::function<void()> change_notification) {self->on_change(change_notification);}
    void add_observer(std::shared_ptr<ms::SurfaceObserver> const& observer) {self->add_observer(observer);}
    void remove_observer(std::shared_ptr<ms::SurfaceObserver> const& observer) {self->remove_observer(observer);}

    // mi::Surface methods
    bool contains(geom::Point const& point) const {return self->contains(point);}

    // mg::Renderable methods
    std::shared_ptr<mg::Buffer> buffer(void const* user_id) const {return self->buffer(user_id);}
    bool alpha_enabled() const {return self->alpha_enabled();}
    geom::Rectangle screen_position() const {return self->screen_position();}
    glm::mat4 transformation() const {return self->transformation();}
    bool visible() const {return self->visible();}
    bool shaped() const {return self->shaped();}
    int buffers_ready_for_compositor() const {return self->buffers_ready_for_compositor();}

private:
    std::shared_ptr<ms::Surface> const self;
    SystemCompositorSession *session;
    int buffer_count;
};

class SystemCompositorShell : public mf::Shell
{
public:
    SystemCompositorShell(SystemCompositor *compositor,
                          std::shared_ptr<mf::Shell> const& self,
                          std::shared_ptr<msh::FocusController> const& focus_controller,
                          std::shared_ptr<ms::SurfaceRanker> const& surface_ranker)
        : compositor{compositor}, self(self), focus_controller{focus_controller}, surface_ranker{surface_ranker}, active_ever_used{false} {}

    std::shared_ptr<mf::Session> session_named(std::string const& name)
    {
        return sessions[name];
    }

    void set_active_session(std::string const& name)
    {
        active_session = name;
        update_session_focus();
    }

    void set_next_session(std::string const& name)
    {
        next_session = name;
        update_session_focus();
    }

    void update_session_focus()
    {
        auto spinner = sessions[spinner_session];
        auto next = sessions[next_session];
        auto active = sessions[active_session];
        std::shared_ptr<SystemCompositorSession> to_show;

        if (spinner)
            spinner->hide();

        if (next && next->is_ready())
        {
            std::cerr << "Setting next focus to session " << next_session;
            next->hide();
            next->raise(surface_ranker);
            to_show = next;
        }
        else if (!next_session.empty() && spinner)
        {
            std::cerr << "Setting next focus to spinner";
            spinner->raise(surface_ranker);
            to_show = spinner;
        }
        else
        {
            std::cerr << "Setting no next focus";
        }

        // If we are booting, we want to wait for next session to be ready to
        // go (it's a smoother experience if user is able to immediately swipe
        // greeter out of way -- enough that it's worth the tiny wait).  So
        // check here to see if next is all ready for us (or we've already
        // focused the active before in which case we're not booting anymore).
        bool next_all_set = next_session.empty() || (next && next->is_ready());
        if (active && active->is_ready() && (next_all_set || active_ever_used))
        {
            std::cerr << "; active focus to session " << active_session;
            focus_controller->set_focus_to(active); // raises and focuses
            active_ever_used = true;
        }
        else if (!active_session.empty() && spinner)
        {
            std::cerr << "; active focus to spinner";
            spinner->show();
            focus_controller->set_focus_to(spinner); // raises and focuses
        }
        else
        {
            std::cerr << "; no active focus";
        }

        if (to_show)
            to_show->show();

        std::cerr << std::endl;
    }

private:
    std::shared_ptr<mf::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<mf::EventSink> const& sink)
    {
        std::cerr << "Opening session " << name << std::endl;

        // We need msh::Session objects because that is what the focus controller
        // works with.  But the mf::Shell interface deals with mf::Session objects.
        // So we cast here since in practice, these objects are also msh::Sessions.
        auto orig = std::dynamic_pointer_cast<msh::Session>(self->open_session(client_pid, name, sink));
        if (!orig)
        {
            std::cerr << "Unexpected non-shell session" << std::endl;
            return std::shared_ptr<mf::Session>();
        }

        auto result = std::make_shared<SystemCompositorSession>(orig, this);
        sessions[name] = result;

        if (client_pid == compositor->get_spinner_pid())
            spinner_session = name;

        return result;
    }

    void close_session(std::shared_ptr<mf::Session> const& session_in)
    {
        std::cerr << "Closing session " << session_in->name() << std::endl;

        auto session = std::dynamic_pointer_cast<SystemCompositorSession>(session_in);
        if (!session)
            return; // shouldn't happen

        if (session->name() == spinner_session)
            spinner_session = "";

        self->close_session(session->get_orig());
        sessions.erase(session->name());
    }

    mf::SurfaceId create_surface_for(
        std::shared_ptr<mf::Session> const& session,
        msh::SurfaceCreationParameters const& params)
    {
        return self->create_surface_for(session, params);
    }

    void handle_surface_created(std::shared_ptr<mf::Session> const& session)
    {
        self->handle_surface_created(session);

        // Opening a new surface will steal focus from our active surface, so
        // restore the focus if needed.
        update_session_focus();
    }

    SystemCompositor *compositor;
    std::shared_ptr<mf::Shell> const self;
    std::shared_ptr<msh::FocusController> const focus_controller;
    std::shared_ptr<ms::SurfaceRanker> const surface_ranker;
    std::map<std::string, std::shared_ptr<SystemCompositorSession>> sessions;
    std::string active_session;
    std::string next_session;
    std::string spinner_session;
    bool active_ever_used;
};

void SystemCompositorSession::mark_ready()
{
    if (!ready)
    {
        ready = true;
        shell->update_session_focus();
    }
}

void SystemCompositorSession::raise(std::shared_ptr<ms::SurfaceRanker> const& controller)
{
    std::map<mf::SurfaceId, std::shared_ptr<SystemCompositorSurface>>::iterator iter;
    for (iter = surfaces.begin(); iter != surfaces.end(); ++iter)
    {
        // This will iterate by creation order, which is fine.  New surfaces on top
        controller->raise(iter->second->get_orig());
    }
}

std::shared_ptr<mf::Surface> SystemCompositorSession::get_surface(mf::SurfaceId surface) const
{
    return surfaces.at(surface);
}

mf::SurfaceId SystemCompositorSession::create_surface(msh::SurfaceCreationParameters const& params)
{
    mf::SurfaceId id = self->create_surface(params);
    std::shared_ptr<mf::Surface> mf_surface = self->get_surface(id);

    auto surface = std::dynamic_pointer_cast<ms::Surface>(mf_surface);
    if (!surface)
    {
        std::cerr << "Unexpected non-scene surface" << std::endl;
        self->destroy_surface(id);
        return mf::SurfaceId(0);
    }

    surfaces[id] = std::make_shared<SystemCompositorSurface>(surface, this);
    return id;
}

class SystemCompositorServerConfiguration : public mir::DefaultServerConfiguration
{
public:
    SystemCompositorServerConfiguration(SystemCompositor *compositor, std::shared_ptr<mo::Configuration> options)
        : mir::DefaultServerConfiguration(options), compositor{compositor}
    {
    }

    int from_dm_fd()
    {
        return the_options()->get("from-dm-fd", -1);
    }

    int to_dm_fd()
    {
        return the_options()->get("to-dm-fd", -1);
    }

    bool show_version()
    {
        return the_options()->is_set("version");
    }

    int power_off_delay()
    {
        return the_options()->get("power-off-delay", 0);
    }

    std::string blacklist()
    {
        auto x = the_options()->get("blacklist", "");
        boost::trim(x);
        return x;
    }

    std::string spinner()
    {
        // TODO: once our default spinner is ready for use everywhere, replace
        // default value with DEFAULT_SPINNER instead of the empty string.
        auto x = the_options()->get("spinner", "");
        boost::trim(x);
        return x;
    }

    bool public_socket()
    {
        return !the_options()->is_set("no-file") && the_options()->get("public-socket", true);
    }

    std::shared_ptr<mi::CursorListener> the_cursor_listener() override
    {
        struct NullCursorListener : public mi::CursorListener
        {
            void cursor_moved_to(float, float) override
            {
            }
        };
        return std::make_shared<NullCursorListener>();
    }

    std::shared_ptr<mir::ServerStatusListener> the_server_status_listener() override
    {
        struct ServerStatusListener : public mir::ServerStatusListener
        {
            ServerStatusListener (SystemCompositor *compositor) : compositor{compositor} {}

            void paused() override
            {
                compositor->pause();
            }

            void resumed() override
            {
                compositor->resume();
            }

            void started() override
            {
            }

            SystemCompositor *compositor;
        };
        return std::make_shared<ServerStatusListener>(compositor);
    }

    std::string get_socket_file()
    {
        // the_socket_file is private, so we have to re-implement it here
        return the_options()->get("file", "/tmp/mir_socket");
    }

    std::shared_ptr<SystemCompositorShell> the_system_compositor_shell()
    {
        return sc_shell([this]
        {
            return std::make_shared<SystemCompositorShell>(
                compositor,
                mir::DefaultServerConfiguration::the_frontend_shell(),
                the_focus_controller(),
                the_surface_ranker());
        });
    }

private:
    mir::CachedPtr<SystemCompositorShell> sc_shell;

    std::shared_ptr<mf::Shell> the_frontend_shell() override
    {
        return the_system_compositor_shell();
    }

    SystemCompositor *compositor;
};

class SystemCompositorConfigurationOptions : public mo::DefaultConfiguration
{
public:
    SystemCompositorConfigurationOptions(int argc, char const* argv[]) :
        DefaultConfiguration(argc, argv)
    {
        add_options()
            ("from-dm-fd", po::value<int>(),  "File descriptor of read end of pipe from display manager [int]")
            ("to-dm-fd", po::value<int>(),  "File descriptor of write end of pipe to display manager [int]")
            ("blacklist", po::value<std::string>(), "Video blacklist regex to use")
            ("version", "Show version of Unity System Compositor")
            ("spinner", po::value<std::string>(), "Path to spinner executable")
            ("public-socket", po::value<bool>(), "Make the socket file publicly writable")
            ("power-off-delay", po::value<int>(), "Delay in milliseconds before powering off screen [int]");
    }

    void parse_config_file(
        boost::program_options::options_description& options_description,
        mo::ProgramOption& options) const override
    {
        options.parse_file(options_description, "unity-system-compositor.conf");
    }
};

bool check_blacklist(std::string blacklist, const char *vendor, const char *renderer, const char *version)
{
    if (blacklist.empty())
        return true;

    std::cerr << "Using blacklist \"" << blacklist << "\"" << std::endl;

    regex_t re;
    auto result = regcomp (&re, blacklist.c_str(), REG_EXTENDED);
    if (result == 0)
    {
        char driver_string[1024];
        snprintf (driver_string, 1024, "%s\n%s\n%s",
                  vendor ? vendor : "",
                  renderer ? renderer : "",
                  version ? version : "");

        auto result = regexec (&re, driver_string, 0, NULL, 0);
        regfree (&re);

        if (result == 0)
            return false;
    }
    else
    {
        char error_string[1024];
        regerror (result, &re, error_string, 1024);
        std::cerr << "Failed to compile blacklist regex: " << error_string << std::endl;
    }

    return true;
}

void SystemCompositor::run(int argc, char **argv)
{
    auto const options = std::make_shared<SystemCompositorConfigurationOptions>(argc, const_cast<char const **>(argv));
    config = std::make_shared<SystemCompositorServerConfiguration>(this, options);

    if (config->show_version())
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return;
    }

    dm_connection = std::make_shared<DMConnection>(io_service, config->from_dm_fd(), config->to_dm_fd());

    struct ScopeGuard
    {
        explicit ScopeGuard(boost::asio::io_service& io_service) : io_service(io_service) {}
        ~ScopeGuard()
        {
            io_service.stop();
            if (io_thread.joinable())
                io_thread.join();
            if (qt_thread.joinable())
                qt_thread.join();
        }

        boost::asio::io_service& io_service;
        std::thread io_thread;
        std::thread qt_thread;
    } guard(io_service);

    mir::run_mir(*config, [&](mir::DisplayServer&)
        {
            auto vendor = (char *) glGetString(GL_VENDOR);
            auto renderer = (char *) glGetString (GL_RENDERER);
            auto version = (char *) glGetString (GL_VERSION);
            std::cerr << "GL_VENDOR = " << vendor << std::endl;
            std::cerr << "GL_RENDERER = " << renderer << std::endl;
            std::cerr << "GL_VERSION = " << version << std::endl;

            if (!check_blacklist(config->blacklist(), vendor, renderer, version))
                throw mir::AbnormalExit ("Video driver is blacklisted, exiting");

            shell = config->the_system_compositor_shell();
            guard.io_thread = std::thread(&SystemCompositor::main, this);
            guard.qt_thread = std::thread(&SystemCompositor::qt_main, this, argc, argv);
        });
}

void SystemCompositor::pause()
{
    std::cerr << "pause" << std::endl;

    if (auto active_session = config->the_focus_controller()->focussed_application().lock())
        active_session->set_lifecycle_state(mir_lifecycle_state_will_suspend);
}

void SystemCompositor::resume()
{
    std::cerr << "resume" << std::endl;

    if (auto active_session = config->the_focus_controller()->focussed_application().lock())
        active_session->set_lifecycle_state(mir_lifecycle_state_resumed);
}

pid_t SystemCompositor::get_spinner_pid() const
{
    return spinner_process.pid();
}

void SystemCompositor::set_active_session(std::string client_name)
{
    std::cerr << "set_active_session" << std::endl;
    shell->set_active_session(client_name);
}

void SystemCompositor::set_next_session(std::string client_name)
{
    std::cerr << "set_next_session" << std::endl;
    shell->set_next_session(client_name);
}

void SystemCompositor::main()
{
    // Make socket world-writable, since users need to talk to us.  No worries
    // about race condition, since we are adding permissions, not restricting
    // them.
    if (config->public_socket() && chmod(config->get_socket_file().c_str(), 0777) == -1)
        std::cerr << "Unable to chmod socket file " << config->get_socket_file() << ": " << strerror(errno) << std::endl;

    dm_connection->set_handler(this);
    dm_connection->start();
    dm_connection->send_ready();

    io_service.run();
}

void SystemCompositor::launch_spinner()
{
    if (config->spinner().empty())
        return;

    // Launch spinner process to provide default background when a session isn't ready
    QStringList env = QProcess::systemEnvironment();
    env << "MIR_SOCKET=" + QString(config->get_socket_file().c_str());
    spinner_process.setEnvironment(env);
    spinner_process.start(config->spinner().c_str());
}

void SystemCompositor::qt_main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DBusScreen dbus_screen(config, config->power_off_delay());
    launch_spinner();
    app.exec();
}
