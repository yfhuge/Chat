#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <fcntl.h>
#include <iostream>
#include <assert.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <thread>

using namespace std;

void epoll_add(int fd, int epollfd, int opt = 0)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (opt) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void RecvFile(int fd)
{
    int fileLen;
    char filename[20] = {0};
    int ret = recv(fd, &fileLen, sizeof(fileLen), 0);
    assert(ret != -1);
    ret = recv(fd, filename, fileLen, 0);
    assert(ret != -1);
    cout << filename << endl;
    char cwd[20] = {0};
    getcwd(cwd, 20);
    string filepwd = std::string(cwd) + "/" + string(filename);
    int newfd = open(filepwd.c_str(), O_CREAT | O_WRONLY, 0666);
    if (newfd == -1)
    {
        perror("open:");
    }
    assert(newfd != -1);
    off_t filesize;
    ret = recv(fd, &filesize, sizeof(filesize), 0);
    assert(ret != -1);
    char buf[1024] = {0};
    int recvlen;
    while (1)
    {
        ret = recv(fd, &recvlen, sizeof(recvlen), 0);
        assert(ret != -1);

        cout << __FILE__ << " " << __LINE__ << " " << recvlen << endl;
        if (recvlen == 0) break;
        ret = recv(fd, buf, recvlen, 0);
        assert(ret != -1);

        ret = write(newfd, buf, recvlen);
        assert(ret != -1);
    }
    close(newfd);
    close(fd);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        cout << "format:./server ip port" << endl;
        return -1;
    }
    string ip = argv[1];
    int port = atoi(argv[2]);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd != -1);
    int reuse = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    assert(ret != -1);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    ret = bind(fd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(fd, 5);
    assert(ret != -1);
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    epoll_add(fd, epollfd);
    struct epoll_event Events[1024];

    while (1)
    {
        int num = epoll_wait(epollfd, Events, 1024, 0);
        for (int i = 0; i < num; ++ i)
        {
            int currfd = Events[i].data.fd;
            if (currfd == fd)
            {
                // 有新连接到来
                struct sockaddr_in cliAddr;
                socklen_t cliAddrLen;
                int clientfd = accept(fd, (struct sockaddr*)&cliAddr, &cliAddrLen);
                epoll_add(clientfd, epollfd, 1);
            }
            else 
            {
                thread recvFileTask(RecvFile, currfd);
                recvFileTask.detach();
            }
        }
    }
    return 0;
}