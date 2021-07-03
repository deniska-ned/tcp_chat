#include "server.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "macrologger.h"
#include "error_codes.h"

#define BUFFER_SIZE     1024
#define IP_ADDR_SIZE    (15 + 1)
#define MAX(l, r) (l) >= (r) ? (l) : (r)


struct ss_node
{
    int fd;
    char ip[IP_ADDR_SIZE];
    struct list_head head;
};

struct message
{
    char *mess_ptr;
    size_t mess_size;
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

    struct server s_tmp = *server;

    rc = s_tmp.master_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (-1 == rc)
    {
        LOG_ERROR("socket for master failed; errno = %d", errno);
        grc = ERR_SERVER_MSOC_FAILED;
    }
    else
    {
        struct sockaddr_in socket_addr;
        socket_addr.sin_family = AF_INET;
        socket_addr.sin_port = htons(s_tmp.master_port);
        socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        rc = bind(s_tmp.master_socket,
                (struct sockaddr *)(&socket_addr),
                sizeof(socket_addr));

        if (-1 == rc)
        {
            LOG_ERROR("bind failed; errno = %d", errno);
            grc = ERR_SERVER_BIND_FAILED;
        }
        else
        {
            set_nonblock(s_tmp.master_socket);

            rc = listen(s_tmp.master_socket, SOMAXCONN);

            if (-1 == rc)
            {
                LOG_ERROR("listen failed; errno = %d", errno);
                grc = ERR_SERVER_BIND_FAILED;
            }
            else
            {
                *server = s_tmp;
                return ERR_OKAY;
                // SUCCESS
            }
        }
    }

    return grc;
}

void server_send_to_other(struct server *server, struct message mes)
{
    struct ss_node *pos;
    list_for_each_entry(pos, &server->slave_sockets_list, head)
    {
        if (mes.sender_node->fd != pos->fd)
        {
            send(pos->fd, mes.mess_ptr, mes.mess_size, MSG_NOSIGNAL);
        }
    }
}

void server_send_to_other_with_ip(struct server *server, struct message mes)
{
    static char buff[IP_ADDR_SIZE + BUFFER_SIZE + 1];
    memset(buff, 0, IP_ADDR_SIZE + BUFFER_SIZE + 1);

    strncpy(buff, mes.sender_node->ip, IP_ADDR_SIZE);
    strncpy(buff + IP_ADDR_SIZE, mes.mess_ptr, mes.mess_size);
    buff[IP_ADDR_SIZE + mes.mess_size] = '\n';

    struct message new_mes = {
        .mess_ptr = buff,
        .mess_size = IP_ADDR_SIZE + mes.mess_size,
        .sender_node = mes.sender_node};

    server_send_to_other(server, new_mes);
}
        
int sockaddrip_to_str(char *buf, struct sockaddr_in client)
{
    int a = (client.sin_addr.s_addr & 0xFF);
    int b = (client.sin_addr.s_addr & 0xFF00) >> 8;
    int c = (client.sin_addr.s_addr & 0xFF0000) >> 16;
    int d = (client.sin_addr.s_addr & 0xFF000000) >> 24;
    int n = sprintf(buf, "[%d.%d.%d.%d]: ", a, b, c, d);
    return n;
}

int server_conn_new_slave(struct server *server)
{
    int rc = ERR_OKAY;

    struct sockaddr_in s_addr;
    socklen_t s_addr_size;

    int new_slave_socket = accept(server->master_socket,
            (struct sockaddr *)(&s_addr), &s_addr_size);

    set_nonblock(new_slave_socket);

    struct ss_node *new_ptr = calloc(1, sizeof(struct ss_node));


    if (NULL == new_ptr)
    {
        LOG_WARNING("%s",
                "memory for new slave socket node isnot allocated");
        rc = ERR_ALLOCATION_FAILED;
    }
    else
    {
        new_ptr->fd = new_slave_socket;
        sockaddrip_to_str(new_ptr->ip, s_addr);

        LOG_INFO("%s -> %s", new_ptr->ip, "connected");

        list_add_tail(&new_ptr->head, &server->slave_sockets_list);


        char buffer[BUFFER_SIZE] = "connected";
        struct message mes = {
            .mess_ptr = buffer,
            .mess_size = strlen(buffer),
            .sender_node = new_ptr};

        server_send_to_other_with_ip(server, mes);
    }

    return rc;
}

int server_process_ss(struct server *server, struct ss_node *node)
{
    int rc = ERR_OKAY;
    static char buffer[BUFFER_SIZE];

    int recv_size = recv(node->fd, buffer, BUFFER_SIZE, MSG_NOSIGNAL);
    buffer[recv_size] = '\0';

    if (recv_size > 0)
    {
        struct message mes = {
            .mess_ptr = buffer,
            .mess_size = recv_size,
            .sender_node = node};

        LOG_TRACE("%s -> %s", node->ip, mes.mess_ptr);

        server_send_to_other_with_ip(server, mes);
    }
    else if (0 == recv_size && EAGAIN != errno)
    {
        // connection is closed
        char mes_data[BUFFER_SIZE] = "disconnected";
        struct message mes = {
            .mess_ptr = mes_data,
            .mess_size = strlen(mes_data),
            .sender_node = node
        };
        server_send_to_other_with_ip(server, mes);

        LOG_INFO("%s -> %s", node->ip, mes.mess_ptr);

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
                server_process_ss(server, ss_node_ptr);
            }
        }

        if (FD_ISSET(server->master_socket, &set))
        {
            server_conn_new_slave(server);
        }
    }
}
