#include <lnl/lnl.h>

class my_listener : public lnl::net_event_listener {
public:
    void on_network_error(const lnl::net_address& endpoint, uint32_t socketErrorCode,
                          const std::string& message) override {
        printf("ERROR: %s\n", message.c_str());
    }

    void on_network_receive(std::shared_ptr<lnl::net_peer>& peer, lnl::net_data_reader& reader, uint8_t channelNumber,
                            lnl::DELIVERY_METHOD deliveryMethod) override {
        static lnl::net_data_writer writer;
        static std::vector<uint8_t> buffer;

        auto remaining = reader.remaining();

        if (reader.size() == 13218) {
            printf("TestFrag: %i, %i\n", reader.data()[reader.position()], reader.data()[reader.position() + 13217]);
        }

        if (buffer.size() != remaining) {
            buffer.resize(remaining, 0);
        }

        if (!reader.try_read(buffer.data(), buffer.size())) {
            printf("Cannot read data\n");
            return;
        }

        peer->send(buffer, deliveryMethod);
    }

    void on_connection_request(std::shared_ptr<lnl::net_connection_request>& request) override {
        request->accept();
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