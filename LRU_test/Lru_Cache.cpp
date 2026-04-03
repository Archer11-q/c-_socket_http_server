//
// Created by Archer on 2026/4/2.
//

#include "Lru_Cache.h"

//更新查询到的节点访问顺序：先移除节点再插入头部
void LRU_Cache::moveToHead(Node* node)
{
    //断开节点在原位置的前后连接
    if (node->prev) node->prev->next=node->next;    //如果节点有前驱，前驱节点的next指向当前节点的后继
    if (node->next) node->next->prev=node->prev;    //如果节点有后继，后继节点的prev指向当前节点的前驱

    //将节点插入到链表头部
    node->next=head;    //当前节点的next指向原头节点
    head->prev=node;    //原头节点的prev指向当前节点
    head=node;          //更新头节点为当前节点
}

//新增节点到链表头部（新插入/新访问）
void LRU_Cache::addToHead(Node* node)
{
    node->next=head;                //新节点的next指向原头节点
    if (head) head->prev=node;      //原节点存在，前驱指向新节点
    head=node;                      //更新头节点为新节点

    //特殊情况：链表为空时，尾节点也指向新节点
    if (!tail) tail=node;
    size++;                            //更新当前缓存大小
}

//删除链表节点（淘汰最久未使用的节点）
void LRU_Cache::removeTail(Node* node)
{
    if (!tail) return;    //链表为空，无需删除

    tail=tail->prev;                    //更新尾节点为当前尾节点的前驱
    if (tail) tail->next=nullptr;       //如果更新后尾节点存在
    else head=nullptr;                  //链表已空，头节点也置空

    //同步删除哈希表中的映射
    map.erase(node->key);                //根据节点key删除哈希表映射
    delete node;                         //释放节点内存
    size--;                              //更新当前缓存大小
}

//根据key查询缓存值，不存在返回-1；存在则更新访问顺序，返回当前节点值
int LRU_Cache::get(int key)
{
    //哈希表中未找到key，返回-1
    if (map.find(key)==map.end()) return -1;

    //找到对应节点
    Node* node=map[key];
    moveToHead(node);    //更新访问顺序：先移除节点再插入头部
    return node->value;  //返回当前节点值
}

//插入/更新键值对，容量满时淘汰尾部节点
void LRU_Cache::put(int key, int value)
{
    //如果key存在，更新value并更新访问顺序
    if (map.find(key)!=map.end())
    {
        Node* node=map[key];    //找到对应节点
        node->value=value;      //更新节点值
        moveToHead(node);       //更新访问顺序：先移除节点再插入头部
        return;
    }

    //key不存在->新建节点并插入头部
    Node* newnode=new Node(key,value);      //创建新节点
    map[key]=newnode;                           //哈希表映射key->新节点
    addToHead(newnode);                         //将新节点插入链表头部

    //缓存超过最大容量->淘汰尾部节点
    if (size>capacity) removeTail(tail);
}

//析构函数：释放链表节点内存，清空哈希表
LRU_Cache::~LRU_Cache()
{
    Node* cur=head;
    while (cur)
    {
        node* temp=cur;   //暂存当前节点指针
        cur=cur->next;    //移动到下一个节点
        delete temp;      //释放当前节点内存
    }
}