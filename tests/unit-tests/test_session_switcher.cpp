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

#include "src/session_switcher.h"
#include "src/spinner.h"

#include "mir/frontend/session.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <vector>
#include <tuple>

namespace
{

class FakeScene
{
public:
    void add(usc::Session* session)
    {
        sessions.emplace_back(session, false);
    }

    void remove(usc::Session* session)
    {
        sessions.erase(find(session));
    }

    void show(usc::Session* session)
    {
        find(session)->second = true;
    }

    void hide(usc::Session* session)
    {
        find(session)->second = false;
    }

    void raise(usc::Session* session)
    {
        auto const iter = find(session);
        auto const pair = *iter;
        sessions.erase(iter);
        sessions.push_back(pair);
    }

    std::vector<std::string> displayed_sessions()
    {
        std::vector<std::string> ret;
        for (auto const& pair : sessions)
        {
            bool session_visible = pair.second;
            if (session_visible)
                ret.push_back(pair.first->name());
        }

        return ret;
    }

private:
    std::vector<std::pair<usc::Session*,bool>> sessions;

    decltype(sessions)::iterator find(usc::Session* session)
    {
        return std::find_if(
            sessions.begin(), sessions.end(),
            [session] (decltype(sessions)::value_type const& p)
            {
                return p.first == session;
            });
    }
};

class StubMirSession : public mir::frontend::Session
{
public:
    StubMirSession(std::string const& name)
        : name_{name}
    {}

    mir::frontend::SurfaceId create_surface(mir::scene::SurfaceCreationParameters const&) override { return mir::frontend::SurfaceId{0}; }
    void destroy_surface(mir::frontend::SurfaceId) override {}
    std::shared_ptr<mir::frontend::Surface> get_surface(mir::frontend::SurfaceId surface) const override { return nullptr; }

    std::string name() const override { return name_; }
    void hide() override {}
    void show() override {}

private:
    std::string const name_;
};

class StubSession : public usc::Session
{
public:
    StubSession(FakeScene& fake_scene, std::string const& name)
        : mir_stub_session{std::make_shared<StubMirSession>(name)},
          fake_scene(fake_scene)
    {
        fake_scene.add(this);
    }

    ~StubSession()
    {
        fake_scene.remove(this);
    }

    std::string name() override
    {
        return mir_stub_session->name();
    }

    void show() override
    {
        fake_scene.show(this);
    }

    void hide() override
    {
        fake_scene.hide(this);
    }

    void raise_and_focus() override
    {
        fake_scene.raise(this);
    }

    bool corresponds_to(mir::frontend::Session const* s) override
    {
        return s == mir_stub_session.get();
    }

    std::shared_ptr<mir::frontend::Session> corresponding_session()
    {
        return mir_stub_session;
    }

private:
    std::shared_ptr<StubMirSession> const mir_stub_session;
    FakeScene& fake_scene;
};

struct StubSpinner : usc::Spinner
{
    void ensure_running() override { is_running_ = true; }
    void kill() override { is_running_ = false; }
    pid_t pid() override { return pid_; }

    void set_pid(pid_t new_pid) { pid_ = new_pid; }
    bool is_running() { return is_running_; }

private:
    bool is_running_ = false;
    pid_t pid_ = 666;
};

struct ASessionSwitcher : testing::Test
{
    std::shared_ptr<StubSession> create_stub_session(std::string const& name)
    {
        return std::make_shared<StubSession>(fake_scene, name);
    }

    std::tuple<std::shared_ptr<StubSession>,std::shared_ptr<StubSession>> boot()
    {
        std::string const boot_active_name{"boot_active"};
        std::string const boot_next_name{"boot_next"};

        auto const boot_active = create_stub_session(boot_active_name);
        auto const boot_next = create_stub_session(boot_next_name);

        switcher.add(boot_active, active_pid);
        switcher.add(boot_next, next_pid);

        switcher.set_next_session(boot_next_name);
        switcher.set_active_session(boot_active_name);
        switcher.mark_ready(boot_active->corresponding_session().get());
        switcher.mark_ready(boot_next->corresponding_session().get());

        return std::make_tuple(boot_active, boot_next);
    }

    FakeScene fake_scene;
    std::shared_ptr<StubSpinner> const stub_spinner{std::make_shared<StubSpinner>()};
    usc::SessionSwitcher switcher{stub_spinner};
    std::string const active_name{"active"};
    std::string const next_name{"next"};
    std::string const spinner_name{"spinner"};
    pid_t const invalid_pid{0};
    pid_t const active_pid{1000};
    pid_t const next_pid{1001};
    pid_t const other_pid{1002};
};

}

TEST_F(ASessionSwitcher, does_not_display_any_session_if_active_and_next_not_set)
{
    using namespace testing;

    switcher.add(create_stub_session("s1"), invalid_pid);
    switcher.add(create_stub_session("s2"), invalid_pid);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, does_not_display_not_ready_active_session)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    switcher.add(active, active_pid);
    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, does_not_display_not_ready_next_session)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);

    switcher.add(next, next_pid);
    switcher.set_next_session(next_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, does_not_display_ready_next_session_without_ready_active_session)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);

    switcher.add(next, next_pid);
    switcher.set_next_session(next_name);
    switcher.mark_ready(next->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher,
       does_not_display_any_session_on_boot_if_not_both_active_and_next_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher,
       displays_the_active_session_on_boot_if_it_is_ready_and_there_is_no_next_session)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_the_active_session_after_boot_if_it_is_ready)
{
    using namespace testing;

    boot();

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_active_over_next_if_both_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());
    switcher.mark_ready(next->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(next_name, active_name));
}


TEST_F(ASessionSwitcher, displays_only_active_if_next_equals_active)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    switcher.add(active, active_pid);

    switcher.set_next_session(active_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_only_active_and_next_sessions)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const other = create_stub_session("other");

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);
    switcher.add(other, other_pid);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());
    switcher.mark_ready(next->corresponding_session().get());
    switcher.mark_ready(other->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(next_name, active_name));
}

TEST_F(ASessionSwitcher, displays_spinner_if_active_is_not_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, does_not_display_spinner_if_next_is_not_ready)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(next, next_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_next_session(next_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, displays_only_spinner_when_active_is_not_ready_but_next_is_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(next->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher,
       displays_only_spinner_when_booting_if_not_both_active_and_next_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher,
       displays_spinner_behind_active_after_boot_if_active_is_ready_but_next_is_not_ready)
{
    using namespace testing;

    boot();

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(next, next_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name, active_name));
}

TEST_F(ASessionSwitcher, starts_and_stops_spinner_as_needed)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    switcher.add(active, active_pid);
    switcher.set_active_session(active_name);

    EXPECT_TRUE(stub_spinner->is_running());

    switcher.mark_ready(active->corresponding_session().get());

    EXPECT_FALSE(stub_spinner->is_running());
}

TEST_F(ASessionSwitcher, does_not_display_next_when_active_is_removed)
{
    using namespace testing;

    std::string const no_session_name;

    std::shared_ptr<StubSession> boot_active;
    std::shared_ptr<StubSession> boot_next;
    std::tie(boot_active, boot_next) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_active->corresponding_session());
    switcher.set_active_session(no_session_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, displays_only_active_not_spinner_when_next_is_removed)
{
    using namespace testing;

    std::string const no_session_name;

    std::shared_ptr<StubSession> boot_active;
    std::shared_ptr<StubSession> boot_next;
    std::tie(boot_active, boot_next) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_next->corresponding_session());
    switcher.set_next_session(no_session_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(boot_active->name()));
}

TEST_F(ASessionSwitcher, displays_spinner_when_active_is_removed_unexpectedly)
{
    using namespace testing;

    std::string const no_session_name;

    std::shared_ptr<StubSession> boot_active;
    std::shared_ptr<StubSession> boot_next;
    std::tie(boot_active, boot_next) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_active->corresponding_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, displays_spinner_under_active_if_next_is_removed_unexpectedly)
{
    using namespace testing;

    std::string const no_session_name;

    std::shared_ptr<StubSession> boot_active;
    std::shared_ptr<StubSession> boot_next;
    std::tie(boot_active, boot_next) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_next->corresponding_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name, boot_active->name()));
}

TEST_F(ASessionSwitcher,
       does_not_display_any_session_when_spinner_is_removed_and_no_sessions_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));

    switcher.remove(spinner->corresponding_session());
    spinner.reset();

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, can_handle_spinner_resurrection_under_different_name)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));

    switcher.remove(spinner->corresponding_session());
    spinner.reset();

    std::string const new_spinner_name{"new_spinner_name"};
    spinner = create_stub_session(new_spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(new_spinner_name));
}

TEST_F(ASessionSwitcher, can_handle_spinner_resurrection_under_different_pid)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());
    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));

    switcher.remove(spinner->corresponding_session());
    spinner.reset();

    pid_t const new_pid{1234};
    stub_spinner->set_pid(new_pid);

    spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, is_not_confused_by_other_session_with_name_of_dead_spinner)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    switcher.remove(spinner->corresponding_session());
    spinner.reset();

    stub_spinner->set_pid(invalid_pid);

    auto const other = create_stub_session(spinner_name);
    switcher.add(other, other_pid);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, is_not_confused_by_other_session_with_pid_of_dead_spinner)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, active_pid);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    auto const old_spinner_pid = stub_spinner->pid();
    stub_spinner->set_pid(invalid_pid);

    switcher.remove(spinner->corresponding_session());
    spinner.reset();

    auto const other = create_stub_session(spinner_name);
    switcher.add(other, old_spinner_pid);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, replaces_tracked_session_if_same_session_name_is_used)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const other = create_stub_session(active_name);
    pid_t const other_pid{2000};

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());

    switcher.add(active, active_pid);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());
    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(active_name));

    switcher.add(other, other_pid);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, ignores_removal_of_untracked_session)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());

    switcher.add(active, active_pid);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_session().get());
    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(active_name));

    auto const other = create_stub_session(active_name);
    switcher.remove(other->corresponding_session());
    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(active_name));
}
