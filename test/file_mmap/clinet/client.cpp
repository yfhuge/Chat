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
    int total = 0, sendLen = 0, lastLen = 0;
    int ret;
    while (total < bufLen)
    {
        ret = send(fd, p + total, bufLen - total, 0);
        if (ret == -1)  return -1;
        total += ret;
        sendLen += ret;
        if (sendLen - lastLen > bufLen / 10000)
        {
            lastLen = sendLen;
            printf("%6.2f%%\r", (double)sendLen * 100 / bufLen);
        }
    }
    return 0;
}

int RecvN(int fd, void *buf, int bufLen)
{
    char *p = (char*)buf;
    int total = 0, recvLen = 0, lastLen = 0;
    int ret;
    while (total < bufLen)
    {
        ret = recv(fd, p + total, bufLen - total, 0);
        if (ret == -1) return -1;
        total += ret;
        recvLen += ret;
        if (recvLen - lastLen > bufLen / 10000)
        {
            lastLen = recvLen;
            printf("%6.2f%%\r", (double)recvLen * 100 / bufLen);
        }
    }
    return 0;
}


int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("format:./client ip port\n");
        return -1;
    }
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd != -1);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);
    int ret = connect(fd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    off_t fileLen;
    int len = recv(fd, &fileLen, sizeof(off_t), 0);
    assert(len != -1);
    int newfd = open("/home/yf/Linux/chat_buf/test/file_mmap/clinet/file", O_CREAT | O_RDWR, 0666);
    assert(newfd != -1);
    ret = ftruncate(newfd, fileLen);
    if (ret == -1) perror("ftrucate error");
    assert(ret != -1);
    void *p = mmap(nullptr, fileLen, PROT_READ | PROT_WRITE, MAP_SHARED, newfd, 0);
    assert(p != (void*)-1);
    ret = RecvN(fd, p, fileLen);
    assert(ret != -1);
    munmap(p, fileLen);
    close(fd);
    close(newfd);
    return 0;
}