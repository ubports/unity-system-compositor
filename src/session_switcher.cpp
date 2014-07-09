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

#include "session_switcher.h"
#include "spinner.h"

usc::SessionSwitcher::SessionSwitcher(std::shared_ptr<Spinner> const& spinner)
    : spinner_process{spinner},
      booting{true}
{
}

void usc::SessionSwitcher::add(std::shared_ptr<Session> const& session, pid_t pid)
{
    std::lock_guard<std::mutex> lock{mutex};

    if (pid == spinner_process->pid())
        spinner_name = session->name();

    sessions[session->name()] = SessionInfo(session);
    update_displayed_sessions();
}

void usc::SessionSwitcher::remove(std::string const& name)
{
    std::lock_guard<std::mutex> lock{mutex};

    if (name == spinner_name)
        spinner_name = "";

    sessions.erase(name);
    update_displayed_sessions();
}

void usc::SessionSwitcher::set_active_session(std::string const& name)
{
    std::lock_guard<std::mutex> lock{mutex};

    active_name = name;
    update_displayed_sessions();
}

void usc::SessionSwitcher::set_next_session(std::string const& name)
{
    std::lock_guard<std::mutex> lock{mutex};

    next_name = name;
    update_displayed_sessions();
}

void usc::SessionSwitcher::mark_ready(mir::scene::Session const* scene_session)
{
    std::lock_guard<std::mutex> lock{mutex};

    for (auto& pair : sessions)
    {
        if (pair.second.session && pair.second.session->corresponds_to(scene_session))
            pair.second.ready = true;
    }

    update_displayed_sessions();
}

void usc::SessionSwitcher::update_displayed_sessions()
{
    hide_uninteresting_sessions();

    bool show_spinner = false;
    ShowMode show_spinner_mode{ShowMode::as_next};
    bool const allowed_to_display_active =
        is_session_ready_for_display(next_name) ||
        !is_session_expected_to_become_ready(next_name) ||
        !booting;

    if (allowed_to_display_active && is_session_ready_for_display(active_name))
    {
        show_session(active_name, ShowMode::as_active);
        booting = false;
    }
    else if (is_session_expected_to_become_ready(active_name))
    {
        show_spinner = true;
        show_spinner_mode = ShowMode::as_active;
    }

    if (!show_spinner)
    {
        if (is_session_ready_for_display(next_name))
        {
            show_session(next_name, ShowMode::as_next);
        }
        else if (is_session_expected_to_become_ready(next_name))
        {
            show_spinner = true;
            show_spinner_mode = ShowMode::as_next;
        }
    }
    else if (is_session_ready_for_display(next_name))
    {
        hide_session(next_name);
    }

    if (show_spinner)
        ensure_spinner_will_be_shown(show_spinner_mode);
    else
        ensure_spinner_is_not_running();
}

void usc::SessionSwitcher::hide_uninteresting_sessions()
{
    for (auto const& pair : sessions)
    {
        if (pair.second.session->name() != active_name &&
            pair.second.session->name() != next_name)
        {
            pair.second.session->hide();
        }
    }
}

bool usc::SessionSwitcher::is_session_ready_for_display(std::string const& name)
{
    auto const iter = sessions.find(name);
    if (iter == sessions.end())
        return false;

    return iter->second.session && iter->second.ready;
}

bool usc::SessionSwitcher::is_session_expected_to_become_ready(std::string const& name)
{
    return !name.empty();
}

void usc::SessionSwitcher::show_session(
    std::string const& name,
    ShowMode show_mode)
{
    auto& session = sessions[name].session;

    if (show_mode == ShowMode::as_active)
        session->raise_and_focus();

    session->show();
}

void usc::SessionSwitcher::hide_session(std::string const& name)
{
    auto& session = sessions[name].session;
    session->hide();
}

void usc::SessionSwitcher::ensure_spinner_will_be_shown(ShowMode show_mode)
{
    auto const iter = sessions.find(spinner_name);
    if (iter == sessions.end())
    {
        spinner_process->ensure_running();
    }
    else
    {
        if (show_mode == ShowMode::as_active)
            iter->second.session->raise_and_focus();
        iter->second.session->show();
    }
}

void usc::SessionSwitcher::ensure_spinner_is_not_running()
{
    spinner_process->kill();
}
