#include <gtest/gtest.h>

#include <lnl/net_manager.h>
#include <lnl/net_event_based_listener.h>

TEST(net_manager, should_connect_ipv4) {
    static constexpr auto MAX_RETRIES = 15;
    static lnl::net_data_writer writer;

    bool isConnected = false;

    lnl::net_event_based_listener serverListener;
    lnl::net_event_based_listener clientListener;

    serverListener.connection_request().subscribe([](auto& request) {
        request->accept();
    });
    clientListener.peer_connected().subscribe([&](auto& peer) {
        isConnected = true;
    });
    serverListener.peer_disconnected().subscribe([&](auto& peer, auto& info) {
        isConnected = false;
    });

    lnl::net_manager server(&serverListener);
    lnl::net_manager client(&clientListener);

    server.name = "server";
    client.name = "client";

    server.start();
    client.start();

    lnl::net_address serverAddress(server.address());
    serverAddress.set_address("localhost");

    client.connect(serverAddress, writer);

    for (int _ = 0; _ < MAX_RETRIES; ++_) {
        client.poll_events();
        server.poll_events();

        if (isConnected &&
            client.first_peer() &&
            client.first_peer()->connection_state() == lnl::CONNECTION_STATE::CONNECTED) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(isConnected);
    ASSERT_TRUE(client.first_peer());
    ASSERT_EQ(client.first_peer()->connection_state(), lnl::CONNECTION_STATE::CONNECTED);
}

TEST(net_manager, should_send_and_receive) {
    static constexpr auto MAX_RETRIES = 15;
    static thread_local lnl::net_data_writer writer;

    bool isReceived = false;
    bool isSent = false;

    const uint32_t data = 0xDEADBEEF;

    lnl::net_event_based_listener serverListener;
    lnl::net_event_based_listener clientListener;

    serverListener.connection_request().subscribe([](auto& request) {
        request->accept();
    });

    clientListener.peer_connected().subscribe([&](auto& peer) {
        writer.reset();

        writer.write(data);

        peer->send(writer, lnl::DELIVERY_METHOD::RELIABLE_ORDERED);

        isSent = true;
    });

    serverListener.network_receive().subscribe([&](auto& peer,
                                                   lnl::net_data_reader& reader,
                                                   auto channel,
                                                   auto method) {
        if (method != lnl::DELIVERY_METHOD::RELIABLE_ORDERED) {
            return;
        }

        auto receivedData = reader.read<uint32_t>();

        isReceived = receivedData == data;
    });

    lnl::net_manager server(&serverListener);
    lnl::net_manager client(&clientListener);

    server.name = "server";
    client.name = "client";

    server.start();
    client.start();

    lnl::net_address serverAddress(server.address());
    serverAddress.set_address("localhost");

    client.connect(serverAddress, writer);

    for (int _ = 0; _ < MAX_RETRIES; ++_) {
        client.poll_events();
        server.poll_events();

        if (server.first_peer() &&
            server.first_peer()->connection_state() == lnl::CONNECTION_STATE::CONNECTED) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(server.first_peer());
    ASSERT_TRUE(server.first_peer()->connection_state() == lnl::CONNECTION_STATE::CONNECTED);

    for (int _ = 0; _ < MAX_RETRIES; ++_) {
        client.poll_events();
        server.poll_events();

        if (isSent && isReceived) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(isSent);
    ASSERT_TRUE(isReceived);
}