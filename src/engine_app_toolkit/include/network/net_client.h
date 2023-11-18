#pragma once

#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"

#include <string>
#include <memory>

namespace engine
{
namespace net
{
    template<typename T>
    class ClientInterface
    {
    public:
        ClientInterface()
        {

        }
        ClientInterface(const ClientInterface&) = delete;
        ClientInterface operator= (const ClientInterface&) = delete;

        virtual ~ClientInterface()
        {
            disconnect();
        }

    public:
        bool connect(const std::string& host, std::uint16_t port)
        {
            try
            {
                asio::ip::tcp::resolver resolver(io_);
                const auto endpoints = resolver.resolve(host, std::to_string(port));

                //connection_ = connection<T>>(connection<T>::owner::client, io_, socket_, messages_in_);
                connection_ = std::move(std::make_unique<Connection<T>>(Connection<T>::owner::client, io_, asio::ip::tcp::socket(io_), messages_in_));
                assert(connection_ != nullptr);

                connection_->connect_to_server(endpoints);

                thread_ = std::thread([this]() { io_.run(); });
            }
            catch (std::exception& e)
            {
                std::cerr << "Connection failed: " << e.what() << std::endl;
                return false;
            }
            return true;
        }

        void disconnect()
        {
            if (connection_ && is_connected())
            {
                connection_->disconnect();
            }

            io_.stop();
            if (thread_.joinable())
            {
                thread_.join();
            }

            connection_.release();
        }

        bool is_connected() const
        {
            return connection_ ? connection_->is_connected() : false;
        }

        ThreadSafeQueue<owned_message<T>>& incoming()
        {
            return messages_in_;
        }

        void send(const Message<T>& msg)
        {
            assert(connection_ != nullptr && "Connection not established! Cant send message.");
            connection_->send(msg);
        }

    protected:
        asio::io_context io_;
        std::thread thread_;

        std::unique_ptr<Connection<T>> connection_;

    private:
        ThreadSafeQueue<owned_message<T>> messages_in_;
    };
}
}