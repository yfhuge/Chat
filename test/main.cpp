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

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        cout << "format:./main ip port" << endl;
        return -1;
    }
    string ip = argv[1];
    int port = atoi(argv[2]);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd != -1);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    int ret = connect(fd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    // 发送文件名
    string filename = "./file", str = "file";
    int filesize = str.size();
    ret = send(fd, &filesize, sizeof(filesize), 0);
    assert(ret != -1);
    ret = send(fd, str.c_str(), str.size(), 0);
    assert(ret != -1);

    int newfd = open(filename.c_str(), O_RDONLY);
    assert(newfd != -1);

    // 发送文件长度
    struct stat filestat;
    stat(filename.c_str(), &filestat);
    ret = send(fd, &filestat.st_size, sizeof(filestat.st_size), 0);
    assert(ret != -1);

    // 发送文件内容
    char buf[1024] = {0};
    int buflen;
    while (1)
    {
        buflen = read(newfd, buf, 1024);
        assert(buflen != -1);

        ret = send(fd, &buflen, sizeof(buflen), 0);
        assert(ret != -1);

        if (buflen == 0) break;

        ret = send(fd, buf, buflen, 0);
        assert(ret != -1);    
    }


    close(fd);
    return 0;
}