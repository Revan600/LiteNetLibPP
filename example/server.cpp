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

        peer->send(writer, lnl::DELIVERY_METHOD::UNRELIABLE);
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