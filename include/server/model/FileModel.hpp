#pragma once

#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <cstdio>

class FileModel
{
public:
    // 查看用户的文件列表
    std::vector<std::string> List(std::string name);

    // 删除指定的文件
    bool DeleteFile(std::string name, std::string file);

    // 创建指定的文件并向文件中追加内容
    void InsertContext(std::string name, std::string file, const std::string &str);
};