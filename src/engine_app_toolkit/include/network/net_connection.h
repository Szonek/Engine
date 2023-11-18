#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace engine
{
namespace net
{
    template<class T>
    class Connection : public std::enable_shared_from_this<Connection<T>>
    {
    public:
        enum class owner
        {
            server = 0,
            client
        };

    public:
        Connection(owner parent, asio::io_context& io, asio::ip::tcp::socket&& socket, ThreadSafeQueue<owned_message<T>>& msgs_in)
            : io_(io)
            , socket_(std::move(socket))
            , messages_in_(msgs_in)
            , parent_(parent)
        {
            assert(parent == owner::client || parent == owner::server);
        }

        virtual ~Connection()
        {

        }

    public:

        std::uint32_t get_id() const
        {
            return id_;
        }

        void connect_to_server(const asio::ip::basic_resolver_results<asio::ip::tcp>& endpoints)
        {
            assert(parent_ == owner::client);
            asio::async_connect(socket_, endpoints, [this](std::error_code ec, asio::ip::tcp::endpoint ep)
                {
                    if (!ec)
                    {
                        std::cout << "[CLIENT] Connected to server at address: " << ep.address() << ", port: " << ep.port() << std::endl;
                        read_header();
                    }
                    else
                    {
                        std::cout << "Error while connecting to server! " << ec.message() << std::endl;
                    }
                }
            );
        }

        void connect_to_client(std::uint32_t id)
        {
            assert(parent_ == owner::server);
            if (is_connected())
            {
                id_ = id;
                // and immeditly start async waiting for any messages to come
                read_header();
            }
        }

        void disconnect()
        {
            if (is_connected())
            {
                asio::post(io_, [this]()
                    {
                        socket_.close();
                    }
                );
            }
        }

        bool is_connected() const
        {
            return socket_.is_open();
        }

        void send(const Message<T>& msg)
        {
            // ToDo: need a bit more time to understand below code
            // Problem which was tried to solive (as I understand it today): write_header() was already run and we could execute second write_header() 
            //   and things could go out of sync, since 2 write_headers() would run in parallel
            asio::post(io_, [this, msg]()
                {
                    // first check if some messages is being currently written out
                    auto asio_already_busy_wiritng_message = !messages_out_.empty();
                    // add msg to queue
                    messages_out_.push_back(msg);
                    if (!asio_already_busy_wiritng_message)
                    {
                        write_header();
                    }

                }
            );
        }

    private:
        // async
        void read_header()
        {
            asio::async_read(socket_, asio::buffer(&message_in_temporary_.header, sizeof(MessageHeader<T>)),
                [this](std::error_code ec, std::size_t bytes_length)
                {
                    if (!ec)
                    {
                        if (message_in_temporary_.header.size > 0)
                        {
                            message_in_temporary_.body.resize(message_in_temporary_.header.size - sizeof(MessageHeader<T>));
                            read_body();
                        }
                        else
                        {
                            add_to_incoming_message_queue();
                        }
                    }
                    else
                    {
                        std::cout << "Read message header fail. Connection id: " << id_ << std::endl;
                        socket_.close();
                    }
                }
            );
        }

        // async
        void read_body()
        {
            asio::async_read(socket_, asio::buffer(message_in_temporary_.body.data(), message_in_temporary_.body.size()),
                [this](std::error_code ec, std::size_t bytes_length)
                {
                    if (!ec)
                    {
                        add_to_incoming_message_queue();
                    }
                    else
                    {
                        std::cout << "Read message body fail. Connection id: " << id_ << std::endl;
                        socket_.close();
                    }
                }
            );
        }

        // async
        void write_header()
        {
            asio::async_write(socket_, asio::buffer(&messages_out_.front().header, sizeof(MessageHeader<T>)),
                [this](std::error_code ec, std::size_t bytes_length)
                {
                    if (!ec)
                    {
                        if (messages_out_.front().body.size() > 0)
                        {
                            write_body();
                        }
                        else
                        {
                            messages_out_.pop_front();
                            if (!messages_out_.empty())
                            {
                                write_header();
                            }
                        }
                    }
                    else
                    {
                        std::cout << "Write message header fail. Connection id: " << id_ << std::endl;
                        socket_.close();
                    }
                }
            );
        }

        // async
        void write_body()
        {
            asio::async_write(socket_, asio::buffer(messages_out_.front().body.data(), messages_out_.front().body.size()),
                [this](std::error_code ec, std::size_t bytes_length)
                {
                    if (!ec)
                    {
                        messages_out_.pop_front();
                        if (!messages_out_.empty())
                        {
                            write_header();
                        }
                    }
                    else
                    {
                        std::cout << "Write message header fail. Connection id: " << id_ << std::endl;
                        socket_.close();
                    }
                }
            );
        }

    private:
        void add_to_incoming_message_queue()
        {
            // server can have many different connections, so we need pointer to connection
            // clients have single connection, so nullptr is enough
            messages_in_.push_back({ parent_ == owner::server ? this->shared_from_this() : nullptr, message_in_temporary_ });

            // kick off another async task which will wait for new header to come
            read_header();
        }

    protected:
        asio::io_context& io_;
        asio::ip::tcp::socket socket_;

        ThreadSafeQueue<Message<T>> messages_out_;
        ThreadSafeQueue<owned_message<T>>& messages_in_;

        Message<T> message_in_temporary_;

        // parent decides on some behaviour of connection
        owner parent_;

        std::uint32_t id_;
    };
}

}