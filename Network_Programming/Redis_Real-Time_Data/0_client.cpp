#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "%s: %s\n", msg, strerror(err));
    abort();
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd<0) {
        die("sochet()");
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    addr.sin_addr.s_addr = inet_addr(INADDR_LOOPBACK); //127.0.0.1
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    char msg[] = "PING\r\n";
    write(fd, msg, sizeof(msg));

    char buf[1024] = {};
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n<1) {
        die("read()");
    }

    printf("Server Sends: %s\n",rbuf);
    close(fd);
    return 0;
}
