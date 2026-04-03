#ifndef CPP_SOCKET_HTTP_SERVER_LRU_CACHE_H
#define CPP_SOCKET_HTTP_SERVER_LRU_CACHE_H

#include<iostream>
#include<unordered_map>


//双向链表结构体：存储缓存键值对，维护前后节点指针
struct Node
{
    int key;	//缓存键：淘汰时需要key删除哈希表映射
    int value;	//缓存值
    Node* prev;	//前驱节点指针
    Node* next;	//后继节点指针

    //初始化双向链表结构体：初始化键值对，前后指针置空
    Node(int k,int v):key(k),value(v),prev(nullptr),next(nullptr) {}
};

class LRU_Cache
{
public:
    //构造函数：初始化缓存容量、当前大小、链表头尾指针
    LRU_Cache(int cap):capacity(cap),size(0),head(nullptr),tail(nullptr) {}

    //析构函数：释放链表节点内存，清空哈希表
    ~LRU_Cache();

    //get函数：根据key查询缓存值，如果不存在返回-1，存在则更新访问顺序，返回当前节点值
    int get(int key);

    //put函数：插入/更新键值对，容量满时淘汰尾部节点
    void put(int key,int value);

private:
    int capacity;	//缓存最大容量，超过该值就淘汰最久未使用的数据
    int size;       //当前缓存中实际存储的元素个数
    Node* head;     //双向链表头节点指针：指向最近使用的节点
    Node* tail;     //双向链表尾节点指针：指向最久未使用的节点，淘汰时从尾部删除
    std::unordered_map<int,Node> map;    //哈希表：key->链表节点，实现O（1）时间查找

    //私有化核心逻辑：1.移除节点再插入头节点、2.新节点出入头部、3.淘汰尾部节点
    void moveToHead(Node* node);

    void addToHead(Node* node);

    void removeTail(Node* node);
};


#endif //CPP_SOCKET_HTTP_SERVER_LRU_CACHE_H