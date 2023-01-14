#pragma once
#include "net_common.h"

namespace engine
{
namespace net
{
    template<typename T>
    struct MessageHeader
    {
        T id{};
        std::uint32_t size = 0;

    };

    template<typename T>
    struct Message
    {
        MessageHeader<T> header{};
        std::vector<std::uint8_t> body;  // ToDo: reserve somes space?

        std::size_t size() const
        {
            return sizeof(MessageHeader<T>) * body.size();
        }

        friend std::ostream& operator << (std::ostream& os, const Message<T>& msg)
        {
            os << "ID: " << static_cast<std::int32_t>(msg.header.id) << " SIZE: " << msg.header.size;
            return os;
        }

        template<typename DataType>
        friend Message<T>& operator << (Message<T>& msg, const DataType& data)
        {
            static_assert(std::is_standard_layout_v<DataType>, "Data is too complext to be pushed!");
            auto i = msg.body.size();
            msg.body.resize(i + sizeof(DataType));
            std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

            msg.header.size = static_cast<std::uint32_t>(msg.body.size() + sizeof(msg.header));

            return msg;
        }

        template<typename DataType>
        friend Message<T>& operator >> (Message<T>& msg, DataType& data)
        {
            static_assert(std::is_standard_layout_v<DataType>, "Data is too complext to be read!");

            size_t i = msg.body.size() - sizeof(DataType);
            std::memcpy(&data, msg.body.data() + i, sizeof(data));

            msg.body.resize(i);

            return msg;
        }
    };

    template<typename T> class Connection;
    template<typename T>
    struct owned_message
    {
        std::shared_ptr<Connection<T>> remote = nullptr;
        Message<T> msg;

        friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
        {
            os << msg.msg;
            return os;
        }

    };

}
}