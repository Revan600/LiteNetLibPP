# LiteNetLib++
C++ port of the famous networking library [LiteNetLib](https://github.com/RevenantX/LiteNetLib) written in C#

[![Made in Ukraine](https://img.shields.io/badge/made_in-ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua) ![ctest](https://github.com/Revan600/LiteNetLibPP/actions/workflows/cmake.yml/badge.svg)
## Build
As of now, library can be built with MSVC on Windows and with GCC on Linux. OS X support is planned in the future releases.

In order to build and run example project use following commands in Powershell for Windows:

    git clone git@github.com:Revan600/LiteNetLibPP.git
    cd LiteNetLibPP
    mkdir build
    cd build
    cmake ../
    cmake --build .
    .\Debug\lnl_example_server.exe
    .\Debug\lnl_example_client.exe

Or in bash for Linux:

    git clone git@github.com:Revan600/LiteNetLibPP.git
    cd LiteNetLibPP
    mkdir build
    cd build
    cmake ../
    make
    ./lnl_example_server
    ./lnl_example_client

After that you should be able to connect to the test server via localhost:4499 (or via your public IP)
## Features
All features of LiteNetLib implemented except of those:

- [ ] NTP
- [ ] Extra packet layers (and thus, no built-in crc32 and encryption)
- [ ] IPv6
- [ ] Connection statistics
- [ ] UDP NAT hole punching
- [ ] Packet loss and latency simulation
- [ ] vcpkg support
- [x] ~~Only MSVC on Windows is currently supported~~

## Performance
A simple ping-pong with 1000 clients (server benchmark):
![enter image description here](https://github.com/Revan600/LiteNetLibPP/blob/main/assets/lnl_perf.png?raw=true)
No packet or connection drops were observed during the tests (but I'm sure there would be some bugs, so feel free to fill the issue)
## Roadmap

- Implement missing features is number 1 priority
- Support all major platforms (linux and osx at least)
- Code cleanup, since this port is almost one-to-one rewrite of the C# version so almost none of C++17 power was used, unfortunately
- Improve overall performance as there's a lot of room to optimize

## Usage
### Server
Here's simple client and server example which was used in the performance tests.
More examples could be found [here](example)

```cpp
#include <lnl/lnl.h>

class my_listener : public lnl::net_event_listener {
public:
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

    void on_connection_request(std::shared_ptr<lnl::net_connection_request>& request) override {
        request->accept();
    }
};

int main() {
    lnl::initialize();
    
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

    void on_connection_request(std::shared_ptr<lnl::net_connection_request>& request) override {
        request->accept();
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

