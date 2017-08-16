/*
 * Copyright © 2013-2014 Canonical Ltd., © 2017 Joseph Rushton Wakeling
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
 *              Alexandros Frantzis <alexandros.frantzis@canonical.com>
 *              Joseph Rushton Wakeling <joe@webdrake.net>
 */

#include <mir/abnormal_exit.h>

#include <boost/exception/all.hpp>
#include <regex.h>
#include <GLES2/gl2.h>

#include <iostream>

namespace
{

bool check_blacklist(
    std::string const& blacklist,
    const char *vendor,
    const char *renderer,
    const char *version)
{
    if (blacklist.empty())
        return true;

    std::cerr << "Using blacklist \"" << blacklist << "\"" << std::endl;

    regex_t re;
    auto result = regcomp(&re, blacklist.c_str(), REG_EXTENDED);
    if (result == 0)
    {
        char driver_string[1024];
        snprintf(driver_string, 1024, "%s\n%s\n%s",
                 vendor ? vendor : "",
                 renderer ? renderer : "",
                 version ? version : "");

        auto result = regexec(&re, driver_string, 0, nullptr, 0);
        regfree (&re);

        if (result == 0)
            return false;
    }
    else
    {
        char error_string[1024];
        regerror(result, &re, error_string, 1024);
        std::cerr << "Failed to compile blacklist regex: " << error_string << std::endl;
    }

    return true;
}

}

namespace usc
{

void validate_video_driver(std::string const& blacklist_regex)
{
    auto vendor = (char *) glGetString(GL_VENDOR);
    auto renderer = (char *) glGetString(GL_RENDERER);
    auto version = (char *) glGetString(GL_VERSION);

    std::cerr << "GL_VENDOR = " << vendor << std::endl;
    std::cerr << "GL_RENDERER = " << renderer << std::endl;
    std::cerr << "GL_VERSION = " << version << std::endl;

    if (!check_blacklist(blacklist_regex, vendor, renderer, version))
    {
        BOOST_THROW_EXCEPTION(
            mir::AbnormalExit("Video driver is blacklisted, exiting"));
    }
}

}
