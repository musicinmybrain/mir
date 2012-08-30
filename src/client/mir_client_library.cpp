/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#include "mir_client/mir_client_library.h"
#include "mir_client/mir_rpc_channel.h"

#include "mir_protobuf.pb.h"

#include <cstddef>

namespace
{

namespace mc = mir::client;
namespace mp = mir::protobuf;

// TODO surface implementation, multiple surfaces per client

class MirClient
{
public:
    MirClient(std::shared_ptr<mc::Logger> const & log)
        : channel("./mir_socket_test", log)
        , server(&channel)
        , surface_created_callback(0)
        , surface_context(0)
    {
    }

    MirClient(MirClient const &) = delete;
    MirClient& operator=(MirClient const &) = delete;

    void create_surface(MirSurface * surface_,
                        MirSurfaceParameters const & params,
                        mir_surface_created_callback callback,
                        void * context)
    {
        client_surface = surface_;
        surface_created_callback = callback;
        surface_context = context;

        mir::protobuf::SurfaceParameters message;
        message.set_width(params.width);
        message.set_height(params.height);
        message.set_pixel_format(params.pixel_format);

        server.create_surface(
            0, &message, &surface,
            google::protobuf::NewCallback(this, &MirClient::surface_created));
    }

    char const * get_error_message()
    {
        return error_message.c_str();
    }

    mp::Surface const & get_surface() const
    {
        return surface;
    }
private:
    void surface_created()
    {
        surface_created_callback(client_surface, surface_context);
    }

    mc::MirRpcChannel channel;
    mp::DisplayServer::Stub server;
    mp::Surface surface;

    std::string error_message;

    MirSurface * client_surface;
    mir_surface_created_callback surface_created_callback;
    void * surface_context;

    std::mutex mutex;
};

}

struct MirConnection
{
    MirClient * client;
};

struct MirSurface
{
    MirClient * client;
};

void mir_connect(mir_connected_callback callback, void * context)
{
    MirConnection * connection = new MirConnection();

    try
    {
        auto log = std::make_shared<mc::ConsoleLogger>();
        connection->client = new MirClient(log);
    }
    catch (std::exception const& /*x*/)
    {
        connection->client = 0; // or Some error object
    }
    
    callback(connection, context);
}

int mir_connection_is_valid(MirConnection * connection)
{
    return connection->client ? 1 : 0;
}

char const * mir_connection_get_error_message(MirConnection * connection)
{
    return connection->client->get_error_message();
}

void mir_create_surface(MirConnection * connection,
                        MirSurfaceParameters const * params,
                        mir_surface_created_callback callback,
                        void * context)
{
    MirSurface * surface = new MirSurface();
    connection->client->create_surface(surface, *params, callback, context);
    surface->client = connection->client;
}

int mir_surface_is_valid(MirSurface *)
{
    return 1;
}

char const * mir_surface_get_error_message(MirSurface * surface)
{
    return surface->client->get_error_message();
}

MirSurfaceParameters mir_surface_get_parameters(MirSurface * surface)
{
    mp::Surface const & sf = surface->client->get_surface();
    return MirSurfaceParameters{sf.width(), sf.height(),
                                static_cast<MirPixelFormat>(sf.pixel_format())};
}

void mir_surface_release(MirSurface *)
{
}

void mir_advance_buffer(MirSurface *,
                        mir_buffer_advanced_callback callback,
                        void * context)
{
    callback(NULL, context);
}   

int mir_buffer_is_valid(MirBuffer *)
{
    return 0;
}

char const * mir_buffer_get_error_message(MirBuffer *)
{
    return "not yet implemented!";
}

int mir_buffer_get_next_vblank_microseconds(MirBuffer *)
{
    return -1;
}
