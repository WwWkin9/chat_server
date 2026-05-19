#include "ChatServer.h"
#include "Config.h"

int main()
{
    Config cfg = Config::loadFromEnvOrFile();
    ChatServer server(cfg);
    return server.run() ? 0 : 1;
}