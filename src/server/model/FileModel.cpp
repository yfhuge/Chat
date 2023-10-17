#include "FileModel.hpp"

// 查看用户的文件列表
std::vector<std::string> FileModel::List(std::string name)
{
    std::vector<std::string> vec;
    
    // 获取文件夹的路径
    char file[64] = {0};
    getcwd(file, 64);
    std::string filename(file);
    filename += "/../file/" + name;

    DIR* dir = opendir(filename.c_str());
    if (dir != nullptr)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)))
        {
            if (entry->d_type == DT_REG)
            {
                vec.push_back(entry->d_name);
            }
        }
    }

    return vec;
}

// 删除指定的文件
bool FileModel::DeleteFile(std::string name, std::string file)
{
    // 获取文件夹的路径
    char cwd[64] = {0};
    getcwd(cwd, 64);
    std::string filename(cwd);
    filename += "/../file/" + name + "/" + file;

    if (access(filename.c_str(), F_OK) == 0)
    {
        if (!std::remove(filename.c_str()))
        {
            return true;
        }
        return false;
    }
    return false;
}


// 创建指定的文件并向文件中追加内容
void FileModel::InsertContext(std::string name, std::string file, const std::string &str)
{
    // 获取文件夹的路径
    char cwd[64] = {0};
    getcwd(cwd, 64);
    std::string filename(cwd);
    filename += "/../file/" + name;

    mkdir(filename.c_str(), 0777);

    std::fstream fstr(filename + "/" + file, std::ios::app);
    fstr << str;
    fstr.close();
}
