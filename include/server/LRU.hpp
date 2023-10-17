#pragma once

#include <unordered_map>

struct DinLink
{
    int key, val;
    DinLink *pre;
    DinLink *next;
    DinLink(int m_key = 0, int m_val = 0)  : key(m_key), val(m_val), pre(nullptr), next(nullptr) {}
};

class LRU
{
public:
    LRU(int capacity);
    void del(int key);
    int put(int key, int val);
private:

    void MoveHead(DinLink *node);
    void InsertHead(DinLink *node);
    int RemoveTail();

    int m_size;
    int max_size;
    std::unordered_map<int, DinLink*> m_mp;
    DinLink *tail;
    DinLink *head;
};