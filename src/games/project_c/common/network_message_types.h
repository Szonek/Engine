#pragma once

#include <network/net_message.h>
#include <network/net_connection.h>

#include <memory>
#include <unordered_map>
#include <array>

namespace project_c
{
using PlayerNetId = std::uint32_t;
constexpr inline const PlayerNetId PlayerNetIdInvalid = -1;

enum class MessageTypes
{
    eToServer_PlayerRegister,
    eToClient_PlayerRegister,
    eToClient_PlayerDisconnected,
    
    eClientServer_PlayerState,
};

std::ostream& operator << (std::ostream& os, const MessageTypes& obj)
{

    static const std::unordered_map<MessageTypes, std::string> map =
    {
        {  MessageTypes::eToServer_PlayerRegister, "eToServer_PlayerRegister"},
        {  MessageTypes::eToClient_PlayerRegister, "eToClient_PlayerRegister"},
        {  MessageTypes::eToClient_PlayerDisconnected, "eToClient_PlayerDisconnected"},

        {  MessageTypes::eClientServer_PlayerState, "eClientServer_PlayerState"},

    };

    if (map.find(obj) != map.end())
    {
        os << map.at(obj);
    }
    else
    {
        os << "unknown!";
    }
    return os;
}


template<MessageTypes MsgId>
struct PayloadBase
{
    static MessageTypes msg_id()
    {
        return MsgId;
    }
};

struct ToServer_PlayerRegister : public PayloadBase<MessageTypes::eToServer_PlayerRegister>
{

};

enum class TileID : std::uint32_t
{
    eUnknown = 0,
    eGrass = 1,
    ePath = 2
};

enum ObstacleID : std::uint32_t
{
    eUnknown = 0,
    eBox = 1
};

class Tile
{
public:
    Tile(TileID id = TileID::eUnknown, ObstacleID obsctale = ObstacleID::eUnknown)
        : tile_id_(id), obstacle_id_(obsctale)
    {

    }

    TileID get_tile_id() const { return tile_id_; }

    void set_tile_id(TileID id)
    {
        tile_id_ = id;
    }

    ObstacleID get_obstacle_id() const { return obstacle_id_; }
    void set_obstacle_id(ObstacleID id)
    {
        obstacle_id_ = id;
    }

    bool can_walk_on() const
    {
        return obstacle_id_ == ObstacleID::eUnknown;
    }

private:
    TileID tile_id_;
    ObstacleID obstacle_id_;
};

struct ToClient_PlayerRegister : public PayloadBase<MessageTypes::eToClient_PlayerRegister>
{
    PlayerNetId id;
    std::array<std::array<Tile, 20>, 20> map;
};

struct ToClient_PlayerDisconnected : public PayloadBase<MessageTypes::eToClient_PlayerDisconnected>
{
    PlayerNetId id;
};

struct ClientServer_PlayerState : public PayloadBase<MessageTypes::eClientServer_PlayerState>
{
    PlayerNetId id;
    std::int32_t coord_x;
    std::int32_t coord_z;

    std::int32_t direction_x;
    std::int32_t direction_z;
};




using NetMessage = engine::net::Message<MessageTypes>;
using NetConnection = engine::net::Connection<MessageTypes>;
using NetConnectionPtr = std::shared_ptr<NetConnection>;
}