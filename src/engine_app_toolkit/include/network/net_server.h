#pragma once

#include "net_common.h"
#include "net_connection.h"
#include "net_tsqueue.h"
#include "net_message.h"

#include <vector>
#include <memory>
#include <deque>

namespace engine
{
namespace net
{

    template<typename T>
    class ServerInterface
    {
    public:
        ServerInterface(std::uint16_t port)
            : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
        {

        }

        virtual ~ServerInterface()
        {
            stop();
        }

        bool start()
        {
            try
            {
                wait_for_client_connection();
                // this has to be after wait, because we need to issue some work first
                // if no work issued than context finishes immeaditly 
                bg_thread_ = std::thread([this]() {io_.run(); });
            }
            catch (std::exception& e)
            {
                std::cout << "[SERVER] Exception: " << e.what() << std::endl;
                return false;
            }

            std::cout << "[SERVER] Started\n";
            return true;
        }

        void stop()
        {
            // request contezt to stop
            io_.stop();

            if (bg_thread_.joinable())
            {
                bg_thread_.join();
            }

            std::cout << "[SERVER] Stopped\n";
        }

        // ASYNC
        void wait_for_client_connection()
        {
            acceptor_.async_accept(
                [this](asio::error_code ec, asio::ip::tcp::socket socket)
                {
                    if (ec)
                    {
                        std::cout << "[SERVER] New connection error: " << ec.message() << "\n";
                    }
                    else
                    {
                        std::cout << "[SERVER] New connection: " << socket.remote_endpoint() << "\n";

                        auto new_conn = std::make_shared<Connection<T>>(Connection<T>::owner::server, io_, std::move(socket), messages_in_);

                        // give user possiblity to reject connection
                        if (on_client_connection(new_conn))
                        {
                            connections_.push_back(std::move(new_conn));

                            connections_.back()->connect_to_client(id_counter_++);

                            std::cout << "[SERVER] .......... [" << connections_.back()->get_id() << "] Connection accepted.\n";
                        }
                        else
                        {
                            std::cout << "[SERVER] .......... Connection denied\n";
                        }
                    }
            // no matter if error or not -> wait for new connection 
            wait_for_client_connection();
                }
            );
        }

        void message_client(std::shared_ptr<Connection<T>> client, const Message<T>& msg)
        {
            assert(client);
            if (client && client->is_connected())
            {
                client->send(msg);
            }
            else
            {
                std::cout << "[SERVER] Sending message to client has failed. Assuming that client has disconnected! \n";
                on_client_disconnect(client);
                client.reset();
                connections_.erase(std::remove(connections_.begin(), connections_.end(), client), connections_.end());
            }
        }

        void message_all_clients(const Message<T>& msg, std::shared_ptr<Connection<T>> ignore_client = nullptr)
        {
            // we dont want to modify "connections_" as we iterate through it
            // create a helper boolean and remove nullpointers after we finished processing "connections_" loop
            bool invalidate_client_exists = false;

            for (auto& client : connections_)
            {
                assert(client);
                if (client && client->is_connected())
                {
                    if (client != ignore_client)
                    {
                        client->send(msg);
                    }
                }
                else
                {
                    on_client_disconnect(client);
                    client.reset();
                    invalidate_client_exists = true;
                }
            }

            if (invalidate_client_exists)
            {
                connections_.erase(std::remove(connections_.begin(), connections_.end(), nullptr), connections_.end());
            }
        }

        void update(std::size_t max_msgs = -1, bool wait = false)
        {
            if (wait)
            {
                messages_in_.wait();
            }
            std::size_t msgs_count = 0;
            while (msgs_count < max_msgs && !messages_in_.empty())
            {
                auto remote_msg = messages_in_.pop_front();
                on_message(remote_msg.remote, remote_msg.msg);

                msgs_count++;
            }
        }

    protected:
        // veto (reject) on connection -> return false
        virtual bool on_client_connection(std::shared_ptr<Connection<T>> client)
        {
            return false;
        }

        virtual void on_client_disconnect(std::shared_ptr<Connection<T>> client)
        {

        }

        virtual void on_message(std::shared_ptr<Connection<T>> client, const Message<T>& msg)
        {

        }

    private:
        ThreadSafeQueue<owned_message<T>> messages_in_;

        std::deque<std::shared_ptr<Connection<T>>> connections_;

        asio::io_context io_;
        std::thread bg_thread_;

        asio::ip::tcp::acceptor acceptor_;

        std::uint32_t id_counter_ = 10000;
    };


}  // namespace net
}