/*
 * Copyright © 2014 Canonical Ltd.
 * Copyright (C) 2020 UBports foundation.
 * Author(s): Ratchanan Srirattanamet <ratchanan@ubports.com>
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
#include "screen.h"

#include <mir/scene/session.h>

usc::SessionSwitcher::SessionSwitcher(std::shared_ptr<Spinner> const& spinner)
    : spinner_process{spinner},
      booting{true},
      has_active_outputs{true},
      wayland{!!getenv("WAYLAND_DISPLAY")}
{
    if (wayland) printf("Using wayland workaround!\n");
}

usc::SessionSwitcher::~SessionSwitcher()
{
    if (auto screen = screen_weak.lock())
        screen->unregister_active_outputs_handler(this);
}

void usc::SessionSwitcher::set_screen(std::shared_ptr<Screen> const& screen)
{
    if (auto old_screen = screen_weak.lock()) // Just in case
        old_screen->unregister_active_outputs_handler(this);

    screen_weak = screen;

    screen->register_active_outputs_handler(this,
        [this](ActiveOutputs const& active_outputs) {
            has_active_outputs =
                (active_outputs.internal + active_outputs.external != 0);
            update_displayed_sessions();
        }
    );
}

void usc::SessionSwitcher::add(std::shared_ptr<Session> const& session, pid_t pid)
{
    std::lock_guard<std::mutex> lock{mutex};

    // WAYLAND HACK
    // Wayland currently does not give us any session name
    if (pid == spinner_process->pid()) {
        sessions[wayland ? "spinner" : session->name()] = SessionInfo(session, true);
    } else {
        sessions[wayland ? "session-0" : session->name()] = SessionInfo(session);
    }

    update_displayed_sessions();
}

void usc::SessionSwitcher::remove(std::shared_ptr<mir::scene::Session> const& session)
{
    std::lock_guard<std::mutex> lock{mutex};

    for (auto spair : sessions) {
        if (spair.second.session == nullptr)
            continue;

        if (spair.second.session->corresponds_to(session.get())) {
            spair.second.session->hide();

            sessions.erase(sessions.find(spair.first));
            update_displayed_sessions();
            return;
        }
    }
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

void usc::SessionSwitcher::mark_ready(mir::scene::Session const* session)
{
    std::lock_guard<std::mutex> lock{mutex};

    for (auto& pair : sessions)
    {
        if (pair.second.session && pair.second.session->corresponds_to(session))
            pair.second.ready = true;
    }

    update_displayed_sessions();
}

void usc::SessionSwitcher::update_displayed_sessions()
{
    if (!has_active_outputs)
        return hide_all_sessions();

    hide_uninteresting_sessions();

    bool show_spinner = false;
    ShowMode show_spinner_mode{ShowMode::as_next};
    bool const allowed_to_display_active =
        is_session_ready_for_display(next_name) ||
        !is_session_expected_to_become_ready(next_name) ||
        !booting;
    bool show_active = false;

    if (allowed_to_display_active && is_session_ready_for_display(active_name))
    {
        show_session(active_name, ShowMode::as_active);
        show_active = true;
        booting = false;
    }
    else if (is_session_expected_to_become_ready(active_name))
    {
        show_spinner = true;
        show_spinner_mode = ShowMode::as_active;
    }

    bool const allowed_to_display_next = !show_spinner && show_active;

    if (allowed_to_display_next)
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

void usc::SessionSwitcher::hide_all_sessions()
{
    for (auto const& pair : sessions)
    {
        pair.second.session->hide();
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
    for (auto spair : sessions) {
        if (spair.second.is_spinner) {
            if (show_mode == ShowMode::as_active)
                spair.second.session->raise_and_focus();
            spair.second.session->show();
            return;
        }
    }

    spinner_process->ensure_running();
}

void usc::SessionSwitcher::ensure_spinner_is_not_running()
{
    spinner_process->kill();
}
