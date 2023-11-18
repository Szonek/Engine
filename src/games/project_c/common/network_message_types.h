#pragma once

#include <network/net_message.h>
#include <network/net_connection.h>

#include <memory>

namespace project_c
{

enum class MessageTypes
{
    eUtilityPing = 0,
    
    eClientPlayerMove,
    eServerPlayerPosition
};

struct ClientPlayerMovePayload
{
    std::uint32_t coord_x;
    std::uint32_t coord_y;
};


using NetMessage = engine::net::Message<MessageTypes>;
using NetConnection = engine::net::Connection<MessageTypes>;
using NetConnectionPtr = std::shared_ptr<NetConnection>;
}