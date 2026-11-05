#ifndef TCP_SERVER_NETWORK_HPP
#define TCP_SERVER_NETWORK_HPP

#include "server_common.hpp"

class NetWork {
    public:
        NetWork() {
            // Do not create a logger during static initialization.
            signal(SIGPIPE, SIG_IGN);
        }
};
static NetWork nw;

#endif


