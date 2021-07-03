#ifndef SERVER_H
#define SERVER_H

#include <inttypes.h>

#include "list.h"

struct server
{
    uint16_t master_port;
    int master_socket;
    struct list_head slave_sockets_list;
};


int set_nonblock(const int fd);

int server_prepare(struct server *server);

int server_run(struct server *server);

#endif // SERVER_H
