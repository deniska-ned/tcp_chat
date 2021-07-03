#include "server.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "macrologger.h"
#include "error_codes.h"


#define BUFFER_SIZE     1024
#define IP_ADDR_SIZE    15    // len(255.255.255.255)
#define MAX(l, r) (l) >= (r) ? (l) : (r)

#define STR_CONNECTED       "connected"
#define STR_DISCONNECTED    "disconnected"


struct ss_node
{
    int fd;
    char ip[IP_ADDR_SIZE + 1];
    struct list_head head;
};

struct message
{
    char *data_ptr;
    size_t size;
    struct ss_node *sender_node;
};


int set_nonblock(const int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return loctl(fd, FIOBIO, &flags);
#endif
}

int server_prepare(struct server *server)
{
    int rc, grc = ERR_OKAY;

    struct server server_cpy = *server;

    rc = server_cpy.master_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == rc)
    {
        LOG_ERROR("socket for master failed; errno = %d", errno);
        grc = ERR_SERVER_MSOC_FAILED;
    }
    else
    {
        struct sockaddr_in socket_addr;
        socket_addr.sin_family = AF_INET;
        socket_addr.sin_port = htons(server_cpy.master_port);
        socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        rc = bind(server_cpy.master_socket, (struct sockaddr *)(&socket_addr),
                sizeof(socket_addr));

        if (-1 == rc)
        {
            LOG_ERROR("bind failed; errno = %d", errno);
            grc = ERR_SERVER_BIND_FAILED;
        }
        else
        {
            set_nonblock(server_cpy.master_socket);

            rc = listen(server_cpy.master_socket, SOMAXCONN);

            if (-1 == rc)
            {
                LOG_ERROR("listen failed; errno = %d", errno);
                grc = ERR_SERVER_BIND_FAILED;
            }
            else
            {
                // SUCCESS
                *server = server_cpy;
                return ERR_OKAY;
            }
        }

        shutdown(server_cpy.master_socket, SHUT_RDWR);
        close(server_cpy.master_socket);
    }

    return grc;
}

void server_send_to_other(const struct server *server,
        const struct message mes)
{
    struct ss_node *pos;
    list_for_each_entry(pos, &server->slave_sockets_list, head)
    {
        if (mes.sender_node->fd != pos->fd)
        {
            send(pos->fd, mes.data_ptr, mes.size, MSG_NOSIGNAL);
        }
    }
}

void server_send_to_other_formated(struct server *server, struct message mes)
{
    static char buffer[IP_ADDR_SIZE + 3 + BUFFER_SIZE + 1];

    // [127.0.0.1] Hello

    size_t curr_i = 0;
    buffer[curr_i++] = '[';

    strncpy(buffer + curr_i, mes.sender_node->ip, IP_ADDR_SIZE);
    size_t ip_len = strlen(mes.sender_node->ip);
    curr_i += ip_len;

    buffer[curr_i++] = ']';

    memset(buffer + curr_i, ' ', (IP_ADDR_SIZE + 2 + 1) - curr_i);
    curr_i += (IP_ADDR_SIZE + 2 + 1) - curr_i;

    memcpy(buffer + curr_i, mes.data_ptr, mes.size);
    curr_i += mes.size;

    if ('\n' != buffer[curr_i - 1])
        buffer[curr_i++] = '\n';

    struct message new_mes = {
        .data_ptr = buffer,
        .size = curr_i,
        .sender_node = mes.sender_node};

    server_send_to_other(server, new_mes);
}
        
int sockaddr_in_ip_to_str(char *buf, struct sockaddr_in client)
{
    int a = (client.sin_addr.s_addr & 0xFF);
    int b = (client.sin_addr.s_addr & 0xFF00) >> 8;
    int c = (client.sin_addr.s_addr & 0xFF0000) >> 16;
    int d = (client.sin_addr.s_addr & 0xFF000000) >> 24;
    int n = sprintf(buf, "%d.%d.%d.%d", a, b, c, d);
    return n;
}

int server_conn_new(struct server *server)
{
    int rc = ERR_OKAY;

    struct sockaddr_in s_addr;
    socklen_t s_addr_size;

    int new_slave_socket = accept(server->master_socket,
            (struct sockaddr *)(&s_addr), &s_addr_size);

    set_nonblock(new_slave_socket);

    struct ss_node *s_node_ptr = calloc(1, sizeof(struct ss_node));


    if (NULL == s_node_ptr)
    {
        LOG_WARNING("%s",
                "memory for new slave socket node isnot allocated");
        rc = ERR_ALLOCATION_FAILED;
    }
    else
    {
        s_node_ptr->fd = new_slave_socket;
        sockaddr_in_ip_to_str(s_node_ptr->ip, s_addr);

        list_add_tail(&s_node_ptr->head, &server->slave_sockets_list);

        LOG_INFO("%s -> %s", s_node_ptr->ip, STR_CONNECTED);

        struct message mes = {
            .data_ptr = STR_CONNECTED,
            .size = strlen(STR_CONNECTED),
            .sender_node = s_node_ptr};

        server_send_to_other_formated(server, mes);
    }

    return rc;
}

int server_proc_ss(struct server *server, struct ss_node *node)
{
    int rc = ERR_OKAY;
    static char buffer[BUFFER_SIZE + 1];

    int recv_size = recv(node->fd, buffer, BUFFER_SIZE, MSG_NOSIGNAL);
    buffer[recv_size] = '\0';

    if (recv_size > 0)
    {
        if ('\n' == buffer[recv_size - 1])
            buffer[recv_size-- - 1] = '\0';

        struct message mes = {
            .data_ptr = buffer,
            .size = recv_size,
            .sender_node = node};

        LOG_TRACE("%s - %s", node->ip, mes.data_ptr);

        server_send_to_other_formated(server, mes);
    }
    else if (0 == recv_size && EAGAIN != errno)
    {
        // connection is closed
        struct message mes = {
            .data_ptr = STR_DISCONNECTED,
            .size = strlen(STR_DISCONNECTED),
            .sender_node = node
        };

        server_send_to_other_formated(server, mes);

        LOG_INFO("%s -> %s", node->ip, STR_DISCONNECTED);

        shutdown(node->fd, SHUT_RDWR);
        close(node->fd);
        list_del(&node->head);

        free(node);

        rc = ERR_CONN_TERMINATED;
    }

    return rc;
}

int server_run(struct server *server)
{
    // list head initialization
    server->slave_sockets_list.prev = &server->slave_sockets_list;
    server->slave_sockets_list.next = &server->slave_sockets_list;

    fd_set set;
    struct ss_node *ss_node_ptr, *ss_node_ptr_02;
    
    while (true)
    {
        FD_ZERO(&set);

        FD_SET(server->master_socket, &set);

        int max_ss_fd = server->master_socket;

        list_for_each_entry(ss_node_ptr, &server->slave_sockets_list, head)
        {
            FD_SET(ss_node_ptr->fd, &set);
            max_ss_fd = MAX(max_ss_fd, ss_node_ptr->fd);
        }
        
        select(max_ss_fd + 1, &set, NULL, NULL, NULL);

        list_for_each_entry_safe(ss_node_ptr, ss_node_ptr_02,
                &server->slave_sockets_list, head)
        {
            int fd = ss_node_ptr->fd;

            if (FD_ISSET(fd, &set))
            {
                server_proc_ss(server, ss_node_ptr);
            }
        }

        if (FD_ISSET(server->master_socket, &set))
        {
            server_conn_new(server);
        }
    }
}
