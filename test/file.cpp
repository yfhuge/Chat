#include <unistd.h>
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

int main()
{
    char filename[100];
    getcwd(filename, 100);
    cout << filename << endl;
    fstream out("/home/yf/test/file", ios::app);
    out << "this is a test";
    return 0;
}