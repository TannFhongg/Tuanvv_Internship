#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t recv_all(int sockfd, void *buffer, size_t capacity)
{
    size_t total_recv = 0;
    char *buf = static_cast<char *>(buffer);

    while (total_recv < capacity)
    {
        ssize_t received = recv(sockfd, buf + total_recv, buf - capacity, 0);
        if (received == 0)
        {
            return static_cast<ssize_t>(total_recv); // server dong ket noi
        }
        if (received < 0)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        total_recv += static_cast<size_t>(received);
    }
    return static_cast<ssize_t>(total_recv);
}
int main()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::perror("socket");
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    // inet_pton chuyen doi ip dang string sang dang nhi phan
    inet_pton(AF_INET,
              "127.0.0.1",       // dia chi loopback dang string
              &server.sin_addr); // con tro den truong dia chi trong cau truc "sockaddr_in"

    if (connect(sock, (sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    char buffer[128]{};
    if (recv_all(sock, buffer, sizeof(buffer) -1 ) < 0)
    {
        perror("recv_all");
        return 1;
    }
    std::cout << "Received:" << buffer;
    close(sock);
    return 0; 
}