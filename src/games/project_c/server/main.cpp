#include <fmt/format.h>

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <deque>
#include <functional>

#include <network/net_server.h>

#include <network_message_types.h>

namespace project_c
{
class ServerProjectC : public engine::net::ServerInterface<MessageTypes>
{
public:
    using engine::net::ServerInterface<MessageTypes>::ServerInterface;


    // veto (reject) on connection -> return false
    bool on_client_connection(NetConnectionPtr client) override
    {
        std::cout << "Client connected: " << client->get_id() << std::endl;
        return true;
    }

    virtual void on_client_disconnect(NetConnectionPtr client)  override
    {
        std::cout << "Client disconnected: " << client->get_id() << std::endl;
    }

    virtual void on_message(NetConnectionPtr client, const NetMessage& msg)  override
    {
        const auto client_id = client->get_id();

        auto msg_temp = msg;

        switch (msg.header.id)
        {
        case MessageTypes::eClientPlayerMove:
        {
            ClientPlayerMovePayload payload{};
            msg_temp >> payload;
            std::cout << "eClientPlayerMove message. [x, y]: [" << payload.coord_x << ", " << payload.coord_y << "]. " << std::endl;
            break;
        }
        default:
            std::cout << "Unknown message type! Msg id: " << static_cast<std::uint32_t>(msg.header.id) << std::endl;
        }
    }
};
}


int main(int argc, char** argv)
{
    try
    {
        project_c::ServerProjectC server(60000);

        while (true)
        {
            server.update(-1, true);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }

	return 0;
}