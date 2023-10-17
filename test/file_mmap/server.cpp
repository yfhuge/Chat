#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

int SendN(int fd, void *buf, int bufLen)
{
    char *p = (char*)buf;
    int total = 0;
    int ret;
    while (total < bufLen)
    {
        ret = send(fd, p + total, bufLen - total, 0);
        if (ret == -1)  return -1;
        total += ret;
    }
    return 0;
}

int RecvN(int fd, void *buf, int bufLen)
{
    char *p = (char*)buf;
    int total = 0;
    int ret;
    while (total < bufLen)
    {
        ret = recv(fd, p + total, bufLen - total, 0);
        if (ret == -1) return -1;
        total += ret;
    }
    return 0;
}


int main(int argc, char** argv) {
    if (argc < 3)
    {
        printf("format:./server ip port\n");
        return -1;
    }
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);

    // Binding the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // Listening
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        return 1;
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        return 1;
    }

    std::string filename = "/home/yf/Linux/chat_buf/test/file_mmap/file";

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("Unable to open file");
        return 1;
    }

    struct stat file_stat;
    stat(filename.c_str(), &file_stat);

    int len = send(new_socket, &file_stat.st_size, sizeof(off_t), 0);
    assert(len != -1);

    void *p = mmap(nullptr, file_stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    assert(p != (void*)-1);

    len = SendN(new_socket, p, file_stat.st_size);
    assert(len != -1);

    munmap(p, file_stat.st_size);
    close(new_socket);
    close(fd);
    close(server_fd);
    
    return 0;
}
