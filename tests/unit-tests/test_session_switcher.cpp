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
        sessions.emplace_back(session, true);
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

class StubSession : public usc::Session
{
public:
    StubSession(FakeScene& fake_scene, std::string const& name)
        : fake_scene(fake_scene),
          name_{name}
    {
        fake_scene.add(this);
    }

    ~StubSession()
    {
        fake_scene.remove(this);
    }

    std::string name() override
    {
        return name_;
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

    bool corresponds_to(mir::scene::Session const* s) override
    {
        return s == corresponding_scene_session();
    }

    mir::scene::Session const* corresponding_scene_session()
    {
        return reinterpret_cast<mir::scene::Session const*>(this);
    }

private:
    FakeScene& fake_scene;
    std::string const name_;
};

struct StubSpinner : usc::Spinner
{
    void ensure_running() { is_running = true; }
    void kill() { is_running = false; }
    pid_t pid() { return 666; }
    bool is_running = false;
};

struct ASessionSwitcher : testing::Test
{
    std::shared_ptr<StubSession> create_stub_session(std::string const& name)
    {
        return std::make_shared<StubSession>(fake_scene, name);
    }

    std::tuple<std::string,std::string> boot()
    {
        std::string const boot_active_name{"boot_active"};
        std::string const boot_next_name{"boot_next"};

        auto const boot_active = create_stub_session(boot_active_name);
        auto const boot_next = create_stub_session(boot_next_name);

        switcher.add(boot_active, 0);
        switcher.add(boot_next, 0);

        switcher.set_next_session(boot_next_name);
        switcher.set_active_session(boot_active_name);
        switcher.mark_ready(boot_active->corresponding_scene_session());
        switcher.mark_ready(boot_next->corresponding_scene_session());

        return std::make_tuple(boot_active_name, boot_next_name);
    }

    FakeScene fake_scene;
    std::shared_ptr<StubSpinner> const stub_spinner{std::make_shared<StubSpinner>()};
    usc::SessionSwitcher switcher{stub_spinner};
    std::string const active_name{"active"};
    std::string const next_name{"next"};
    std::string const spinner_name{"spinner"};
};

}

TEST_F(ASessionSwitcher, does_not_display_any_session_if_active_and_next_not_set)
{
    using namespace testing;

    switcher.add(create_stub_session("s1"), 0);
    switcher.add(create_stub_session("s2"), 0);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, does_not_display_not_ready_active_session)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    switcher.add(active, 0);
    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, does_not_display_not_ready_next_session)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);

    switcher.add(next, 0);
    switcher.set_next_session(next_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, displays_ready_next_session)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);

    switcher.add(next, 0);
    switcher.set_next_session(next_name);
    switcher.mark_ready(next->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(next_name));
}

TEST_F(ASessionSwitcher,
       does_not_display_any_session_on_boot_if_not_both_active_and_next_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, 0);
    switcher.add(next, 0);

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher,
       displays_the_active_session_on_boot_if_it_is_ready_and_there_is_no_next_session)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_the_active_session_after_boot_if_it_is_ready)
{
    using namespace testing;

    boot();

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, 0);
    switcher.add(next, 0);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_active_over_next_if_both_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, 0);
    switcher.add(next, 0);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_scene_session());
    switcher.mark_ready(next->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(next_name, active_name));
}


TEST_F(ASessionSwitcher, displays_only_active_if_next_equals_active)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);

    switcher.add(active, 0);

    switcher.set_next_session(active_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(active_name));
}

TEST_F(ASessionSwitcher, displays_only_active_and_next_sessions)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const other = create_stub_session("other");

    switcher.add(active, 0);
    switcher.add(next, 0);
    switcher.add(other, 0);

    switcher.set_next_session(next_name);
    switcher.set_active_session(active_name);
    switcher.mark_ready(active->corresponding_scene_session());
    switcher.mark_ready(next->corresponding_scene_session());
    switcher.mark_ready(other->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(next_name, active_name));
}

TEST_F(ASessionSwitcher, displays_spinner_if_active_is_not_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, displays_spinner_if_next_is_not_ready)
{
    using namespace testing;

    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(next, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_next_session(next_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, displays_only_spinner_when_active_is_not_ready_but_next_is_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);
    auto const spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(next, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(next->corresponding_scene_session());

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

    switcher.add(active, 0);
    switcher.add(next, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_scene_session());

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

    switcher.add(active, 0);
    switcher.add(next, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);
    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_scene_session());

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name, active_name));
}

TEST_F(ASessionSwitcher, starts_and_stops_spinner_as_needed)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto const next = create_stub_session(next_name);

    switcher.add(active, 0);
    switcher.add(next, 0);

    switcher.set_active_session(active_name);

    EXPECT_TRUE(stub_spinner->is_running);

    switcher.set_next_session(next_name);
    switcher.mark_ready(active->corresponding_scene_session());
    switcher.mark_ready(next->corresponding_scene_session());

    EXPECT_FALSE(stub_spinner->is_running);
}

TEST_F(ASessionSwitcher, displays_next_when_active_is_removed)
{
    using namespace testing;

    std::string const no_session_name;

    std::string boot_active_name;
    std::string boot_next_name;
    std::tie(boot_active_name, boot_next_name) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_active_name);
    switcher.set_active_session(no_session_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(boot_next_name));
}

TEST_F(ASessionSwitcher, displays_active_when_next_is_removed)
{
    using namespace testing;

    std::string const no_session_name;

    std::string boot_active_name;
    std::string boot_next_name;
    std::tie(boot_active_name, boot_next_name) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_next_name);
    switcher.set_next_session(no_session_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(boot_active_name));
}

TEST_F(ASessionSwitcher, displays_spinner_when_active_is_removed_unexpectedly)
{
    using namespace testing;

    std::string const no_session_name;

    std::string boot_active_name;
    std::string boot_next_name;
    std::tie(boot_active_name, boot_next_name) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name));
}

TEST_F(ASessionSwitcher, displays_spinner_under_active_if_next_is_removed_unexpectedly)
{
    using namespace testing;

    std::string const no_session_name;

    std::string boot_active_name;
    std::string boot_next_name;
    std::tie(boot_active_name, boot_next_name) = boot();

    auto const spinner = create_stub_session(spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    switcher.remove(boot_next_name);

    EXPECT_THAT(fake_scene.displayed_sessions(),
                ElementsAre(spinner_name, boot_active_name));
}

TEST_F(ASessionSwitcher,
       does_not_display_any_session_when_spinner_is_removed_and_no_sessions_are_ready)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));

    spinner.reset();
    switcher.remove(spinner_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}

TEST_F(ASessionSwitcher, can_handle_spinner_resurrection_under_different_name)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(spinner_name));

    spinner.reset();
    switcher.remove(spinner_name);

    std::string const new_spinner_name{"new_spinner_name"};
    spinner = create_stub_session(new_spinner_name);
    switcher.add(spinner, stub_spinner->pid());

    EXPECT_THAT(fake_scene.displayed_sessions(), ElementsAre(new_spinner_name));
}

TEST_F(ASessionSwitcher, is_not_confused_by_other_session_with_name_of_dead_spinner)
{
    using namespace testing;

    auto const active = create_stub_session(active_name);
    auto spinner = create_stub_session(spinner_name);

    switcher.add(active, 0);
    switcher.add(spinner, stub_spinner->pid());

    switcher.set_active_session(active_name);

    spinner.reset();
    switcher.remove(spinner_name);

    auto const other = create_stub_session(spinner_name);
    switcher.add(other, 0);

    EXPECT_THAT(fake_scene.displayed_sessions(), IsEmpty());
}
