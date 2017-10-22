/*
 * Copyright © 2017 Joseph Rushton Wakeling
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
 * Authored by: Joseph Rushton Wakeling <joe@webdrake.net>
 */

#ifndef USC_EXTRACT_SOCKET_CONFIG_H
#define USC_EXTRACT_SOCKET_CONFIG_H

#include <functional>

namespace mir
{
class Server;
}

namespace usc
{

class ExtractSocketConfig
{
public:
    using Callback = std::function<void(std::string, bool)>;

    explicit ExtractSocketConfig(Callback const& callback);
    ~ExtractSocketConfig();

    void operator()(mir::Server& server) const;

private:
    Callback callback;
};

}

#endif // USC_EXTRACT_SOCKET_CONFIG_H
