#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t send_all(int sockfd, const void *buffer, size_t length)
{
    size_t total_sent = 0;
    const char *buf = static_cast<const char *>(buffer);

    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, buf + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            if (sent < 0 && errno == EINTR)
            {
                continue;
            }
            return -1; // loi khac
        }
        total_sent += static_cast<size_t>(sent);
    }
    return static_cast<ssize_t>(total_sent);
}
int main()
{
    int server_fd = socket(AF_INET // a protocal family
                           ,
                           SOCK_STREAM // a transport protocal (socket type)
                           ,
                           0 // the default protocol for this combination
    );
    if (server_fd < 0)
    {
        std::perror("socket");
        return 1;
    }

    // Address Representation in Code

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);       // chuẩn chung cho Internet, luôn là big-endian.
    addr.sin_addr.s_addr = INADDR_ANY; // lang nghe tren moi dia chi
    /* a port identifies a specific service or application running on that host
    Ports are 16-bit values, allowing a range from 0 to 65535.
    */

    if (bind(server_fd, // socket file descriptor
             (sockaddr *)&addr,
             sizeof(addr)) < 0) // Kernel cần biết độ dài để đọc đúng dữ liệu địa chỉ.
    {
        std::perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0)
    {
        std::perror("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server listenning on port 8080 \n";

    /*
This call returns a new socket descriptor. The original socket remains in the listening state,
while the new socket represents a specific client connection.
This distinction is critical:
• the listening socket accepts new clients,
• the connected socket exchanges data with one client
*/
    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0)
    {
        std::perror("accept");
        close(server_fd);
        return 1;
    }

    const char msg[] = "Hello world from server\n";

    if (send_all(client_fd, msg, sizeof(msg) - 1) < 0)
    {
        perror("total_sent fail");
    }
    close(client_fd);
    close(server_fd);

    return 0; 
}