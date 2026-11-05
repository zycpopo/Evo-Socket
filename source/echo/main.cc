#include "echo.hpp"

int main()
{
    loopforge::LogConfig logging;
    logging.name = "echo";
    logging.file_prefix = "./logs/echo-";
    loopforge::InitLogging(logging);
    INF_LOG("Echo server starting on port %d", 8500);
    EchoServer server(8500);
    server.Start();
    return 0;
}
