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
template<std::uint32_t WIDTH, std::uint32_t HEIGHT>
class ServerProjectC : public engine::net::ServerInterface<MessageTypes>
{
public:
    ServerProjectC(std::uint16_t port)
        : ServerInterface<MessageTypes>(port)
    {


        for (auto i = 0; i < HEIGHT; i++)
        {
            for (auto j = 0; j < WIDTH; j++)
            {
                auto& tile = map_[i][j];
                tile = Tile(TileID::eGrass);
                if (j == WIDTH / 2)
                {
                    tile.set_tile_id(TileID::ePath);
                }
            }
        }
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 1].set_obstacle_id(ObstacleID::eBox);
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 2].set_obstacle_id(ObstacleID::eBox);
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 3].set_obstacle_id(ObstacleID::eBox);

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
        const auto client_id = client->get_id();
        std::cout << "Client disconnected: " << client_id << std::endl;
        // client connection will be closed here, allow for it, because we can send message here, client may be invalid already!
        // in on_message(..) we will inform clients about disconnection (deffered)
        if (players_roaster_.find(client_id) != players_roaster_.end())
        {
            players_disconnect_list_.push_back(client_id);
        }
        else
        {
            std::cout << "[SERVER] Cant find " << client_id << " in player roaster map!" << std::endl;
        }
    }

    virtual void on_message(NetConnectionPtr client, const NetMessage& msg)  override
    {
        const auto client_id = client->get_id();

        auto msg_temp = msg;
        std::cout << "[SERVER] Processing msg: " << msg_temp << ", id: " << msg_temp.header.id << std::endl;

        // first try to process all disconnections
        {
            for (auto& dc : players_disconnect_list_)
            {
                players_roaster_.erase(dc);

                ToClient_PlayerDisconnected payload{};
                payload.id = dc;

                NetMessage msg{};
                msg.header.id = MessageTypes::eToClient_PlayerDisconnected;
                msg << payload;
                message_all_clients(msg);
            }
            players_disconnect_list_.clear();
        }


        switch (msg.header.id)
        {
        case MessageTypes::eToServer_PlayerRegister:
        {
            // register player
            {
                ToClient_PlayerRegister send_register{};
                send_register.id = client_id;
                send_register.map = map_;

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
    std::array<std::array<Tile, WIDTH>, HEIGHT> map_;
    std::unordered_map<PlayerNetId, ClientServer_PlayerState> players_roaster_;
    std::deque<PlayerNetId> players_disconnect_list_;
};
}


int main(int argc, char** argv)
{
    try
    {
        project_c::ServerProjectC<20, 20> server(60000);

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