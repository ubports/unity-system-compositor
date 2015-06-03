/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef USC_SESSION_SWITCHER_H_
#define USC_SESSION_SWITCHER_H_

#include "dm_connection.h"
#include "session_monitor.h"

#include <map>
#include <mutex>

namespace usc
{
class Spinner;

class SessionSwitcher : public DMMessageHandler, public SessionMonitor
{
public:
    explicit SessionSwitcher(std::shared_ptr<Spinner> const& spinner);

    void add(std::shared_ptr<Session> const& session, pid_t pid) override;
    void remove(std::shared_ptr<mir::frontend::Session> const& session) override;
    void mark_ready(mir::frontend::Session const* session) override;

    /* From DMMessageHandler */
    void set_active_session(std::string const& name) override;
    void set_next_session(std::string const& name) override;

private:
    enum class ShowMode { as_active, as_next };

    void update_displayed_sessions();
    void hide_uninteresting_sessions();
    bool is_session_ready_for_display(std::string const& name);
    bool is_session_expected_to_become_ready(std::string const& name);
    void show_session(std::string const& name, ShowMode show_mode);
    void hide_session(std::string const& name);
    void ensure_spinner_will_be_shown(ShowMode show_mode);
    void ensure_spinner_is_not_running();

    struct SessionInfo
    {
        SessionInfo() = default;
        explicit SessionInfo(std::shared_ptr<Session> session)
            : session{session}
        {
        }
        std::shared_ptr<Session> session;
        bool ready = false;
    };

    std::mutex mutex;
    std::shared_ptr<Spinner> const spinner_process;
    std::map<std::string, SessionInfo> sessions;
    std::string active_name;
    std::string next_name;
    std::string spinner_name;
    bool booting;
};

}

#endif

