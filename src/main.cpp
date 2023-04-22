#include <lnl/lnl.h>

struct my_data {

};

int main() {
    lnl::net_manager server;
    server.start(4499);

    while (true) {

    }

    return 0;
}