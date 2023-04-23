#include <lnl/lnl.h>

#ifdef WIN32

#endif

class my_listener : public lnl::net_event_listener {
    int32_t m_received_count = 0;
public:
    void on_peer_connected(std::shared_ptr<lnl::net_peer>& peer) override {
        printf("[Client] connected to: %s\n", peer->endpoint().to_string().c_str());

        lnl::net_data_writer dataWriter;

        for (int i = 0; i < 5; ++i) {
            dataWriter.reset();
            dataWriter.write(0);
            dataWriter.write(i);
            peer->send(dataWriter, lnl::DELIVERY_METHOD::RELIABLE_UNORDERED);

            dataWriter.reset();
            dataWriter.write(1);
            dataWriter.write(i);
            peer->send(dataWriter, lnl::DELIVERY_METHOD::RELIABLE_ORDERED);

            dataWriter.reset();
            dataWriter.write(2);
            dataWriter.write(i);
            peer->send(dataWriter, lnl::DELIVERY_METHOD::SEQUENCED);

            dataWriter.reset();
            dataWriter.write(3);
            dataWriter.write(i);
            peer->send(dataWriter, lnl::DELIVERY_METHOD::UNRELIABLE);

            dataWriter.reset();
            dataWriter.write(4);
            dataWriter.write(i);
            peer->send(dataWriter, lnl::DELIVERY_METHOD::RELIABLE_SEQUENCED);
        }

        //And test fragment
        std::vector<uint8_t> testData;
        testData.resize(13218, 0);
        testData[0] = 192;
        testData[13217] = 31;
        peer->send(testData, lnl::DELIVERY_METHOD::RELIABLE_ORDERED);
    }

    void on_peer_disconnected(std::shared_ptr<lnl::net_peer>& peer, lnl::disconnect_info& disconnectInfo) override {
        printf("[Client] disconnected: %i\n", (int32_t) disconnectInfo.reason);
    }

    void on_network_error(const lnl::net_address& endpoint, uint32_t socketErrorCode,
                          const std::string& message) override {
        printf("[Client] error: %s\n", message.c_str());
    }

    void on_network_receive(std::shared_ptr<lnl::net_peer>& peer, lnl::net_data_reader& reader, uint8_t channelNumber,
                            lnl::DELIVERY_METHOD deliveryMethod) override {
        if (reader.size() == 13218) {
            printf("TestFrag: %i, %i\n", reader.data()[reader.position()], reader.data()[reader.position() + 13217]);
        } else {
            auto type = reader.read<int32_t>();
            auto num = reader.read<int32_t>();

            m_received_count++;

            printf("CNT: %i, TYPE: %i, NUM: %i, MTD: %i\n", m_received_count, type, num, (int32_t) deliveryMethod);
        }
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

    static lnl::net_data_writer writer;

    my_listener listener;
    lnl::net_manager client(&listener);
    lnl::net_address address("127.0.0.1", 4499);

    client.start();
    client.connect(address, writer);

    while (client.is_running()) {
        client.poll_events();
    }

    return 0;
}