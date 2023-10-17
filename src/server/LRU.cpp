#include "LRU.hpp"

LRU::LRU(int capacity)
{
    head = new DinLink();
    tail = new DinLink();
    head->next = tail;
    tail->pre = head;
    m_size = 0;
    max_size = capacity;
}

void LRU::MoveHead(DinLink *node)
{
    node->next->pre = node->pre;
    node->pre->next = node->next;
    node->pre = head;
    head->next->pre = node;
    node->next = head->next;
    head->next = node;
}

void LRU::InsertHead(DinLink *node)
{
    node->pre = head;
    node->next = head->next;
    head->next->pre = node;
    head->next = node;
}

int LRU::RemoveTail()
{
    DinLink *node = tail->pre;
    tail->pre = node->pre;
    node->pre->next = tail;
    int tmp = node->key;
    m_mp.erase(tmp);
    delete node;
    return tmp;
}

void LRU::del(int key)
{
    if (!m_mp.count(key)) return;
    DinLink *node = m_mp[key];
    node->next->pre = node->pre;
    node->pre->next = node->next;
    m_size --;
    delete node;
}

int LRU::put(int key, int val)
{
    if (m_mp.count(key))
    {
        DinLink *node = m_mp[key];
        MoveHead(node);
        node->val = val;
    }
    else 
    {
        if (m_size == max_size)
        {
            DinLink *node = new DinLink(key, val);
            int tmp = RemoveTail();
            InsertHead(node);
            m_mp.insert({key, node});
            return tmp;
        }
        else 
        {
            m_size ++;
            DinLink *node = new DinLink(key, val);
            InsertHead(node);
            m_mp.insert({key, node});
        }
    }
    return -1;
}

