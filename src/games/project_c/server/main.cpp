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

enum class Messages
{
    ePing = 0
};

class ServerProjectC : public engine::net::ServerInterface< Messages>
{
public:
    using engine::net::ServerInterface<Messages>::ServerInterface;
};

int main(int argc, char** argv)
{
    try
    {
        ServerProjectC server(60000);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }

	return 0;
}