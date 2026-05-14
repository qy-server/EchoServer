#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static bool readn(int fd, void *buf, size_t n)
{
    char *p = static_cast<char *>(buf);
    size_t left = n;
    while (left > 0)
    {
        ssize_t nr = recv(fd, p, left, 0);
        if (nr <= 0) return false;
        p += nr;
        left -= nr;
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage: ./client ip port\n");
        printf("example: ./client 127.0.0.1 5085\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(static_cast<uint16_t>(atoi(argv[2])));
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) != 1)
    {
        printf("invalid ip: %s\n", argv[1]);
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr *>(&servaddr), sizeof(servaddr)) != 0)
    {
        printf("connect(%s:%s) failed: %s\n", argv[1], argv[2], strerror(errno));
        close(sockfd);
        return -1;
    }

    printf("connect ok.\n");
    printf("start time: %ld\n", static_cast<long>(time(nullptr)));

    char body[1024];
    char packet[1100];
    for (int ii = 0; ii < 10000; ++ii)
    {
        int bodylen = snprintf(body, sizeof(body), "message %d", ii);
        if (bodylen <= 0) break;

        memcpy(packet, &bodylen, 4);
        memcpy(packet + 4, body, static_cast<size_t>(bodylen));
        if (send(sockfd, packet, static_cast<size_t>(bodylen + 4), 0) <= 0)
        {
            perror("send");
            close(sockfd);
            return -1;
        }

        int replylen = 0;
        if (!readn(sockfd, &replylen, 4))
        {
            printf("server closed while reading header.\n");
            close(sockfd);
            return -1;
        }
        if (replylen <= 0 || replylen >= static_cast<int>(sizeof(body)))
        {
            printf("bad reply length: %d\n", replylen);
            close(sockfd);
            return -1;
        }
        if (!readn(sockfd, body, static_cast<size_t>(replylen)))
        {
            printf("server closed while reading body.\n");
            close(sockfd);
            return -1;
        }
        body[replylen] = 0;
    }

    printf("end time: %ld\n", static_cast<long>(time(nullptr)));
    close(sockfd);
    return 0;
}
