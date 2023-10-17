#include "../include/public.pb.h"
#include <iostream>

using namespace std;

int main()
{
    fixbuf::Info info;
    info.set_type(fixbuf::ADD_FRIEND_MSG);
    info.set_info("ttttttttttt");
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return -1;
    }
    std::cout << "send info : " << info_buf << endl << endl;
    fixbuf::Friend msg;
    msg.set_friendid(1);
    msg.set_userid(2);
    std::string str;
    if (!msg.SerializeToString(&str))
    {
        std::cerr << "serialize error!" << std::endl;
        return -1; 
    }
    cout << "msg : " << str << endl;
    return 0;

}