/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 */

#ifndef USC_ASIO_DM_CONNECTION_H_
#define USC_ASIO_DM_CONNECTION_H_

#include "dm_connection.h"

#include <boost/asio.hpp>
#include <thread>

namespace usc
{

class AsioDMConnection : public DMConnection
{
public:
    AsioDMConnection(
        int from_dm_fd, int to_dm_fd,
        std::shared_ptr<DMMessageHandler> const& dm_message_handler);
    ~AsioDMConnection();

    void start() override;

private:
    enum class USCMessageID
    {
        ping = 0,
        pong = 1,
        ready = 2,
        session_connected = 3,
        set_active_session = 4,
        set_next_session = 5,
    };

    void send_ready();
    void read_header();
    void on_read_header(const boost::system::error_code& ec);
    void on_read_payload(const boost::system::error_code& ec);
    void send(USCMessageID id, std::string const& body);

    boost::asio::io_service io_service;
    boost::asio::posix::stream_descriptor from_dm_pipe;
    boost::asio::posix::stream_descriptor to_dm_pipe;
    std::thread io_thread;
    std::shared_ptr<DMMessageHandler> const dm_message_handler;

    static size_t const size_of_header = 4;
    unsigned char message_header_bytes[size_of_header];
    boost::asio::streambuf message_payload_buffer;
    std::vector<char> write_buffer;

};

}

#endif /* USC_ASIO_DM_CONNECTION_H_ */
