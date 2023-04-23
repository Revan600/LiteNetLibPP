# LiteNetLib++
C++ port of the famous networking library [LiteNetLib](https://github.com/RevenantX/LiteNetLib) written in C#

[![Made in Ukraine](https://img.shields.io/badge/made_in-ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)
## Build
As of now, library can be built only with MSVC on Windows. Linux and OS X support is planned in the future releases.

In order to build and run example project use following commands in Powershell:

    git clone git@github.com:Revan600/LiteNetLibPP.git
    cd LiteNetLibPP
    mkdir build
    cd build
    cmake -G "Visual Studio 17 2022" ../
    cmake --build .
    .\Debug\lnl.exe
After that you should be able to connect to the test server via localhost:4499
## Features
All features of LiteNetLib implemented except of those:

 - [ ] NTP
 - [ ] Extra packet layers (and thus, no built-in crc32 and encryption)
 - [ ] IPv6
 - [ ] Connection statistics
 - [ ] UDP NAT hole punching
 - [ ] Packet loss and latency simulation
 - [ ] Only MSVC on Windows is currently supported

## Performance
A simple ping-pong with 1000 clients (LNL++ as server):
![enter image description here](https://github.com/Revan600/LiteNetLibPP/blob/main/assets/lnl_perf.png?raw=true)
No packet or connection drops were observed during the tests (but I'm sure there would be some bugs, so feel free to fill the issue)
## Roadmap

 - Implement missing features is number 1 priority
 - Support all major platforms (linux and osx at least)
 - Code cleanup, since this port is almost one-to-one rewrite of the C# version so almost none of C++17 power was used, unfortunately
 - Improve overall performance as there's a lot of room to optimize
 
 ## Usage
 ### Server
 Here's simple server example which was used in the performance tests
 
```cpp
#include <lnl/lnl.h>

class my_listener : public lnl::net_event_listener {
public:
    void on_peer_connected(std::shared_ptr<lnl::net_peer>& peer) override {

    }

    void on_peer_disconnected(std::shared_ptr<lnl::net_peer>& peer, lnl::disconnect_info& disconnectInfo) override {

    }

    void on_network_error(const lnl::net_address& endpoint, uint32_t socketErrorCode,
                          const std::string& message) override {
        printf("ERROR: %s\n", message.c_str());
    }

    void on_network_receive(std::shared_ptr<lnl::net_peer>& peer, lnl::net_data_reader& reader, uint8_t channelNumber,
                            lnl::DELIVERY_METHOD deliveryMethod) override {
        static lnl::net_data_writer writer;

        int32_t value;

        if (!reader.try_read(value)) {
            printf("cannot read value\n");
            return;
        }

        writer.reset();

        value++;

        writer.write(value);

        peer->send(writer, lnl::DELIVERY_METHOD::RELIABLE_ORDERED);
    }

    void on_network_receive_unconnected(const lnl::net_address& endpoint, lnl::net_data_reader& reader,
                                        lnl::UNCONNECTED_MESSAGE_TYPE messageType) override {

    }

    void on_network_latency_update(std::shared_ptr<lnl::net_peer>& peer, int latency) override {

    }

    void on_connection_request(std::shared_ptr<lnl::net_connection_request>& request) override {
        request->accept();
    }

    void on_message_delivered(std::shared_ptr<lnl::net_peer>& peer, void* userData) override {

    }
};

int main() {
    my_listener listener;
    lnl::net_manager server(&listener);
    server.start(4499);

    while (server.is_running()) {
        server.poll_events();
    }

    return 0;
}
```

### Client

```cpp
#include <lnl/lnl.h>

class my_listener : public lnl::net_event_listener {
public:
    void on_peer_connected(std::shared_ptr<lnl::net_peer>& peer) override {

    }

    void on_peer_disconnected(std::shared_ptr<lnl::net_peer>& peer, lnl::disconnect_info& disconnectInfo) override {

    }

    void on_network_error(const lnl::net_address& endpoint, uint32_t socketErrorCode,
                          const std::string& message) override {
        printf("ERROR: %s\n", message.c_str());
    }

    void on_network_receive(std::shared_ptr<lnl::net_peer>& peer, lnl::net_data_reader& reader, uint8_t channelNumber,
                            lnl::DELIVERY_METHOD deliveryMethod) override {
        static lnl::net_data_writer writer;

        int32_t value;

        if (!reader.try_read(value)) {
            printf("cannot read value\n");
            return;
        }

        writer.reset();

        printf("%i\n", value);

        value++;

        writer.write(value);

        peer->send(writer, lnl::DELIVERY_METHOD::RELIABLE_ORDERED);
    }

    void on_network_receive_unconnected(const lnl::net_address& endpoint, lnl::net_data_reader& reader,
                                        lnl::UNCONNECTED_MESSAGE_TYPE messageType) override {

    }

    void on_network_latency_update(std::shared_ptr<lnl::net_peer>& peer, int latency) override {

    }

    void on_connection_request(std::shared_ptr<lnl::net_connection_request>& request) override {
        request->accept();
    }

    void on_message_delivered(std::shared_ptr<lnl::net_peer>& peer, void* userData) override {

    }
};

int main() {
    lnl::initialize();

    my_listener listener;
    lnl::net_manager client(&listener);
    lnl::net_address address("localhost", 4499);

    lnl::net_data_writer writer;

    client.start();
    client.connect(address, writer);

    while (client.is_running()) {
        client.poll_events();
    }

    return 0;
}
```

## Credits
 - [RevenantX](https://github.com/RevenantX) - the author of the original library
 - [Revan600](https://github.com/Revan600) - me, for porting

