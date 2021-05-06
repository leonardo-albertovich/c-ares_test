#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <ares.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>

void ares_query_cb(void *arg, int status, int timeouts, struct hostent *host)
{
    printf("ares_query_cb\n");

    if(!host || status != ARES_SUCCESS){
        printf("Failed to lookup %s\n", ares_strerror(status));
        return;
    }

    printf("Found address name %s\n", host->h_name);
    char ip[INET6_ADDRSTRLEN];
    int i = 0;

    for (i = 0; host->h_addr_list[i]; ++i) {
        inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));
        printf("%s\n", ip);
    }
}

static void wait_ares_select(ares_channel channel)
{

    while(1) {
        struct timeval *tvp, tv;
        fd_set read_fds, write_fds;
        int nfds;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        nfds = ares_fds(channel, &read_fds, &write_fds);
        if(nfds == 0){
            break;
        }
        tvp = ares_timeout(channel, NULL, &tv);
        select(nfds, &read_fds, &write_fds, NULL, tvp);
        ares_process(channel, &read_fds, &write_fds);
    }
}

static void wait_ares_epoll(ares_channel channel)
{
    struct epoll_event epoll_events[ARES_GETSOCK_MAXNUM];
    ares_socket_t      sockets[ARES_GETSOCK_MAXNUM];
    int                socket_count;
    int                event_count;
    int                epoll_fd;
    // fd_set             readers;
    // fd_set             writers;
    int                result;
    int                index;
    // int                nfds;
    struct timeval    *tvp;
    struct timeval     tv;

    while (1) {
        result = ares_getsock(channel, sockets, ARES_GETSOCK_MAXNUM);

        printf("[%d]\n", result);

        if (result == 0){
            break;
        }

        // nfds = ares_fds(channel, &readers, &writers);

        epoll_fd = epoll_create1(0);

        if (epoll_fd == -1) {
            fprintf(stderr, "Failed to create epoll file descriptor\n");
            break;
        }

        tvp = ares_timeout(channel, NULL, &tv);

        for (index = 0 ; ; index++) {
            epoll_events[index].events = 0;
            // epoll_events[index].fd = -1;

            if (0 != ARES_GETSOCK_READABLE(result, index)) {
                epoll_events[index].events |= EPOLLIN;
            }

            if (0 != ARES_GETSOCK_WRITABLE(result, index)) {
                epoll_events[index].events |= EPOLLOUT;
            }

            if (0 != epoll_events[index].events) {
                epoll_events[index].events |= EPOLLERR | EPOLLHUP | EPOLLRDHUP;
                epoll_events[index].data.fd = sockets[index];

                printf("Added %d to epoll events\n", sockets[index]);

                if (0 != epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockets[index], &epoll_events[index])) {
                    fprintf(stderr, "Failed to add file descriptor to epoll\n");
                    break;
                }
            }
            else
            {
                break;
            }
        }

        socket_count = index;

        if (0 == socket_count) {
            break;
        }

        memset(epoll_events, 0, sizeof(epoll_events));

        event_count = epoll_wait(epoll_fd, epoll_events, socket_count, 30000); //change timeout

        if (close(epoll_fd)) {
            fprintf(stderr, "Failed to close epoll file descriptor\n");
            break;
        }

        epoll_fd = -1;

        for (index = 0 ; index < event_count ; index++) {
            printf("Calling ares_process for fd %d\n", epoll_events[index].data.fd);
            ares_process_fd(channel, epoll_events[index].data.fd, epoll_events[index].data.fd);
        }
    }

    if (-1 != epoll_fd) {
        if (close(epoll_fd)) {
            fprintf(stderr, "Failed to close epoll file descriptor\n");
        }

        epoll_fd = -1;
    }

}

int main(int argc, char **argv)
{
    int result;

    result = ares_library_init(ARES_LIB_INIT_ALL);

    if (0 != result) {
        printf("error: ares_library_init\n");
        return -1;
    }

    {
        ares_channel query_channel;

        result = ares_init(&query_channel);

        if (ARES_SUCCESS != result) {
            printf("error: ares_init\n");
            return -2;
        }

        ares_gethostbyname(query_channel, "pedo.com", AF_INET, ares_query_cb, NULL);

    // sleep(10);
        // wait_ares_select(query_channel);
        wait_ares_epoll(query_channel);

        ares_destroy(query_channel);
    }


    return 0;
}