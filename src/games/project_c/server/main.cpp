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
    ServerProjectC(std::uint16_t port)
        : ServerInterface<MessageTypes>(port)
    {
        start();
    }

    virtual ~ServerProjectC()
    {
        stop();
    }

    // veto (reject) on connection -> return false
    bool on_client_connection(NetConnectionPtr client) override
    {
        std::cout << "Client connected: " << *client << std::endl;
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
        std::cout << "[SERVER] Processing msg: " << msg_temp << ", id: " << msg_temp.header.id << std::endl;

        switch (msg.header.id)
        {
        case MessageTypes::eToServer_PlayerRegister:
        {
            // register player
            {
                ToClient_PlayerRegister send_register{};
                send_register.id = client_id;

                NetMessage msg_out{};
                msg_out.header.id = MessageTypes::eToClient_PlayerRegister;
                msg_out << send_register;
                message_client(client, msg_out);
            }
            // update player with current roaster
            for (const auto& payload : players_roaster_)
            {
                NetMessage msg_out{};
                msg_out.header.id = MessageTypes::eClientServer_PlayerState;
                msg_out << payload;
                message_client(client, msg_out);
            }

            // update roaster with the new player (inform all players that the player has connected)
            {
                ClientServer_PlayerState payload{};
                payload.id = client_id;
                players_roaster_[payload.id] = payload;

                NetMessage msg_out{};
                msg_out.header.id = MessageTypes::eClientServer_PlayerState;
                msg_out << payload;
                message_all_clients(msg_out, client);
            }


            break;
        }
        case MessageTypes::eClientServer_PlayerState:
        {
            // update all clients with new state
            ClientServer_PlayerState new_state{};
            msg_temp >> new_state;
            players_roaster_[client_id] = new_state;
            message_all_clients(msg, client);
            break;
        }

        default:
            std::cout << "[SERVER] Unknown message type! Msg id: " << static_cast<std::uint32_t>(msg.header.id) << std::endl;
        }
    }

private:
    std::unordered_map<PlayerNetId, ClientServer_PlayerState> players_roaster_;
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