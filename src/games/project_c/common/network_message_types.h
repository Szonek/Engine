#pragma once

#include <network/net_message.h>
#include <network/net_connection.h>

#include <memory>
#include <unordered_map>

namespace project_c
{
using PlayerNetId = std::uint32_t;
constexpr inline const PlayerNetId PlayerNetIdInvalid = -1;

enum class MessageTypes
{
    eToServer_PlayerRegister,
    eToClient_PlayerRegister,
    
    eClientServer_PlayerState,
};

std::ostream& operator << (std::ostream& os, const MessageTypes& obj)
{

    static const std::unordered_map<MessageTypes, std::string> map =
    {
        {  MessageTypes::eToServer_PlayerRegister, "eToServer_PlayerRegister"},
        {  MessageTypes::eToClient_PlayerRegister, "eToClient_PlayerRegister"},

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

struct ToClient_PlayerRegister : public PayloadBase<MessageTypes::eToClient_PlayerRegister>
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