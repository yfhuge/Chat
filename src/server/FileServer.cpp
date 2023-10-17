#include "FileServer.hpp"

int SendN(int fd, void *buf, off_t bufLen)
{
    char *p = (char*)buf;
    off_t total = 0;
    int ret;
    while (total < bufLen)
    {
        ret = send(fd, p + total, bufLen - total, 0);
        if (ret == -1)  return -1;
        total += ret;
    }
    return 0;
}

int RecvN(int fd, void *buf, off_t bufLen)
{
    char *p = (char*)buf;
    off_t total = 0;
    int ret;
    while (total < bufLen)
    {
        ret = recv(fd, p + total, bufLen - total, 0);
        if (ret == -1) return -1;
        total += ret;
    }
    return 0;
}

// 将文件描述符加入到epoll监听中或者从epoll中删除监听
void EpollCtl(int fd, int epollfd, int opt)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (opt) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

FileServer* FileServer::GetInstance()
{
    static FileServer instance;
    return &instance; 
}

FileServer::FileServer() { }

// 设置IP和端口
void FileServer::Init(const std::string &ip, int port)
{
    m_ip = ip;
    m_port = port;
}

// 获取IP
std::string FileServer::GetIp()
{
    return m_ip;
}

// 获取port
int FileServer::GetPort()
{
    return m_port;
}

// 接收文件的线程
void FileServer::RecvFileTask(int fd)
{
    char flag;
    int len = recv(fd, &flag, sizeof(flag), 0);
    assert(len != -1);

    if (flag == 1)
    {
        // 接收文件

        // 接收用户名
        int userNameLen;
        char userName[32] = {0};
        len = recv(fd, &userNameLen, sizeof(userNameLen), 0);
        assert(len != -1);
        len = recv(fd, userName, userNameLen, 0);
        assert(len != -1);

        // 接收文件名
        int fileNameLen;
        len = recv(fd, &fileNameLen, sizeof(fileNameLen), 0);
        assert(len != -1);
        char fileName[20] = {0};
        len = recv(fd, fileName, fileNameLen, 0);
        assert(len != -1);

        // 接收文件的长度
        off_t fileLength;
        len = recv(fd, &fileLength, sizeof(fileLength), 0);

        assert(len != -1);

        // 找到文件的路径
        char pwd[32] = {0};
        getcwd(pwd, 32);
        std::string file(pwd);
        file += "/../file/" + std::string(userName) + "/" + std::string(fileName);

        int newfd = open(file.c_str(), O_CREAT | O_RDWR, 0666);
        assert(newfd != -1);

        // 接收文件内容
        int ret = ftruncate(newfd, fileLength);
        assert(ret != -1);
        void *p = mmap(nullptr, fileLength, PROT_READ | PROT_WRITE, MAP_SHARED, newfd, 0);
        assert(p != (void*)-1);
        ret = RecvN(fd, p, fileLength);
        assert(ret != -1);
        munmap(p, fileLength);
        
        std::cout << "send success" << std::endl;
        close(newfd);
    }
    else 
    {
        // 发送文件

        // 接收用户名
        int userNameLen;
        len = recv(fd, &userNameLen, sizeof(userNameLen), 0);
        assert(len != -1);
        char userName[32] = {0};
        len = recv(fd, userName, userNameLen, 0);
        assert(len != -1);

        // 接收文件名
        int fileLen;
        len = recv(fd, &fileLen, sizeof(fileLen), 0);
        assert(len != -1);
        char fileName[32] = {0};
        len = recv(fd, fileName, fileLen, 0);
        assert(len != -1);

        char cwd[32] = {0};
        getcwd(cwd, 32);
        std::string filecwd = std::string(cwd) + "/../file/" + std::string(userName) + "/" + std::string(fileName);
        std::cout << __FILE__ << " " << filecwd << std::endl;
        int newfd = open(filecwd.c_str(), O_RDONLY);
        if (newfd == -1) perror("open error");
        assert(newfd != -1);

        // 发送文件长度
        struct stat fileStat;
        stat(filecwd.c_str(), &fileStat);
        len = send(fd, &fileStat.st_size, sizeof(fileStat.st_size), 0);
        assert(len != -1);

        // 发送文件内容
        void *p = mmap(nullptr, fileStat.st_size, PROT_READ, MAP_SHARED, newfd, 0);
        assert(p != (void*)-1);
        int ret = SendN(fd, p, fileStat.st_size);
        assert(ret != -1);
        munmap(p, fileStat.st_size);
        std::cout << "recv success!" << std::endl;

        close(newfd);
    }
    close(fd);
}

void FileServer::Start()
{
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_fd != -1);

    int reuse = 1;
    int ret = setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    assert(ret != -1);
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = inet_addr(m_ip.c_str());
    ret = bind(m_fd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(m_fd, 5);
    assert(ret != -1);

    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    EpollCtl(m_fd, m_epollfd);

    while (1)
    {
        int num = epoll_wait(m_epollfd, m_events, MAX_EPOLL_SIZE, 0);
        for (int i = 0; i < num; ++ i)
        {
            int curr_fd = m_events[i].data.fd;
            if (curr_fd == m_fd)
            {
                // 有新连接到来
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen;
                int m_clientfd = accept(m_fd, (struct sockaddr*)&clientAddr, &clientAddrLen);
                assert(m_clientfd != -1);
                EpollCtl(m_clientfd, m_epollfd, 1);
            }
            else 
            {
                // 有客户端发送消息
                std::thread RecvFile(RecvFileTask, curr_fd);
                RecvFile.detach();
            }
        }
    }
}