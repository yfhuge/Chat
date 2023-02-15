#ifndef _GROUPUSER_H_
#define _GROUPUSER_H_

#include "user.hpp"
#include <string>
using namespace std;

class GroupUser : public User {
public:
    void setRole(string role) { this->role = role; }
    string getRole() { return this->role; }
private:
    string role;
};

#endif