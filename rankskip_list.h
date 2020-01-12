//
//  Created by dguco on 20-1-9.
//  http://download.redis.io/redis-stable/src/t_set.c
//

#ifndef SERVER_SKIP_LIST_H
#define SERVER_SKIP_LIST_H

#include <unordered_map>
#include <cmath>
using namespace std;

#define ZSKIPLIST_MAXLEVEL 64 /* Should be enough for 2^64 elements */
#define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */


template <typename SCORE_TYPE,typename MEMBER_TYPE>
struct skiplistNode
{
    skiplistNode(int level)
    {
        this->backward = NULL;
        this->level = level;
        this->levels = new zskiplistLevel[level];
        for (int i = 0;i < level;i++)
        {
            this->levels[i].forward = NULL;
            this->levels[i].span = 0;
        }
    }

    ~skiplistNode()
    {
        if (levels) delete[] levels;
        levels = NULL;
        backward = NULL;
    }
    SCORE_TYPE score;  //排序key，必须是一个可以比较的类型，必须实现对象的比较操作符(一般位数值类型(long,int,double等))
    MEMBER_TYPE ele;  //分数对应的对象信息,必须是一个可以比较的类型，必须实现对象的比较操作符
    skiplistNode *backward; //第0层的回溯指针
    //每层的节点信息
    struct zskiplistLevel
    {
        skiplistNode<SCORE_TYPE,MEMBER_TYPE> *forward;  //当前层当前节点下一个节点
        unsigned long span; //该层中当前节点与下一结点之间相隔几个节点(在level[0]中的结点数,level[0]节点是连续的该值为0)
    };
    int level;
    zskiplistLevel *levels;
};

template <typename SCORE_TYPE,typename MEMBER_TYPE>
struct skiplist
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *header, *tail; //header和tail分别指向头结点和尾结点，
    unsigned long length; //结点数量，
    int level; //level为表中结点的最高等级。
};

template <typename SCORE_TYPE,typename MEMBER_TYPE>
class CRankSkipList
{
public:
    /**
     * 注意初始化的时候调表的头部指针已经存在但是长度为0
     */
    CRankSkipList(unsigned long maxLen = 0)
    {
        mskiplist.level = 1;
        mskiplist.length = 0;
        mskiplist.header = new skiplistNode<SCORE_TYPE,MEMBER_TYPE>(ZSKIPLIST_MAXLEVEL);
        mskiplist.tail = NULL;
        mrankMap.clear();
        mrankMaxLen = maxLen;
    }

    ~CRankSkipList()
    {
        skiplistNode<SCORE_TYPE,MEMBER_TYPE> *node = mskiplist.header->levels[0].forward, *next;
        delete mskiplist.header;
        while(node) {
            next = node->levels[0].forward;
            delete node;
            node = next;
        }
    }

    /**
     * 插入一个新的节点如果ele已存在，则更新积分
     * @param zsl
     * @param score
     * @param ele
     * @return 插入的skiplistNode
     */
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *InsertOrUpdateNode(SCORE_TYPE score, MEMBER_TYPE ele);
    /**
     * 删除一个节点
     * @param score
     * @param ele
     * @return 1 ok 0 not found
     */
    int DeleteNode(MEMBER_TYPE ele);
    /**
     *
     * @param start
     * @param end
     * @param dict
     * @return 删除rank排名的节点
     */
    unsigned long DeleteRangeByRank(unsigned long rank);
    /**
     *
     * @param start
     * @param end
     * @param dict
     * @return 删除排名范围内的节点[start,end]
     */
    unsigned long DeleteRangeByRank(unsigned long start, unsigned long end);
    /**
     * update一个节点
     * @param curscore
     * @param ele
     * @param newscore
     * @return 插入的skiplistNode
     */
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *UpdateScore(SCORE_TYPE newscore,MEMBER_TYPE ele);
    /**
     * 获取排名
     * @param ele
     * @return 0 不存在 [1,2^64] rank
     */
    unsigned long Rank(MEMBER_TYPE ele);
    /**
     *获取指定排名的节点info
     * @param zsl
     * @param rank
     * @return
     */
    skiplistNode<SCORE_TYPE,MEMBER_TYPE>* GetNodeByRank(unsigned long rank);
private:
    /**
     * 创建一个节点
     * @param level
     * @param score
     * @param value
     * @return
     */
    skiplistNode<SCORE_TYPE,MEMBER_TYPE>* CreateSkipListNode(int level,SCORE_TYPE score,MEMBER_TYPE value);
    /**
     * 删除节点
     * @param deleteNode
     * @param update
     */
    void DeleteNode(skiplistNode<SCORE_TYPE,MEMBER_TYPE> *deleteNode, skiplistNode<SCORE_TYPE,MEMBER_TYPE> **update);

    //随机跳表的层数
    int RandomLevel(void)
    {
        int level = 1;
        while ((random() & 0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
            level += 1;
        return (level < ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
    }
private:
    typedef typename std::unordered_map<MEMBER_TYPE,skiplistNode<SCORE_TYPE,MEMBER_TYPE>*>::iterator rank_it;
    //tips:最初想把rank也缓存在map里，但是发现插入一个节点很可能会导致很多甚至所有的节点的排名都发生变化，缓存失效的概率太大
    std::unordered_map<MEMBER_TYPE,skiplistNode<SCORE_TYPE,MEMBER_TYPE>*> mrankMap;
    skiplist<SCORE_TYPE,MEMBER_TYPE> mskiplist;
    unsigned long mrankMaxLen;//排行榜最大长度 0 不限制
};

template <typename SCORE_TYPE,typename MEMBER_TYPE>
skiplistNode<SCORE_TYPE,MEMBER_TYPE> *CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::InsertOrUpdateNode(SCORE_TYPE score, MEMBER_TYPE ele)
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *update[ZSKIPLIST_MAXLEVEL] = {0}; //每一层需要修改的结点,在每层中，新的节点需要插入在该节点的后面
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *tmpNode;
    unsigned int rank[ZSKIPLIST_MAXLEVEL];  //每一层要修改的结点的排名(根据score从小到大排序)
    int i, level;

    rank_it it = mrankMap.find(ele);
    //如果节点已存在，先删除老的节点
    if(it != mrankMap.end())
    {
        return UpdateScore(score,ele);
    }

    tmpNode = mskiplist.header;
    for (i = mskiplist.level-1; i >= 0; i--)
    {
        //初始化 update数组和rank数组
        rank[i] = i == (mskiplist.level - 1) ? 0 : rank[i+1];
        //不可能出现这种情况，如果出现则说明调表结构已经被破坏,排行榜已经乱了
        if (!(i < tmpNode->level))
        {
            printf("The data of skiplist is bad\n");
            return NULL;
        }
        //如果当前节点的下一个节点is not null，且他的下一个节点的score小于插入节点的score，或者他们积分相等但是ele小于插入节点的ele，则继续往后找，直到
        //找到条件不满足的跳出循环，则新插入的节点需要插入该层的当前节点后面，把此节点记录到update数组中
        while (tmpNode->levels[i].forward &&
                (tmpNode->levels[i].forward->score > score ||
                    (tmpNode->levels[i].forward->score == score && tmpNode->levels[i].forward->ele > ele)))
        {
            rank[i] += tmpNode->levels[i].span;  //该层中当前节点与下一结点之间在level[0]中的结点数累计的结果即每一层要修改的结点的排名
            tmpNode = tmpNode->levels[i].forward;
        }
        update[i] = tmpNode;
    }

    //如果长度==最大长度
    if (mrankMaxLen > 0 && mskiplist.length == mrankMaxLen)
    {
        //如果更新的节点是最后一个(分最低),且新的分数比当前分数低直接返回，不进榜
        if (update[0]->levels[0].forward == NULL || tmpNode->levels[0].forward->score > score)
        {
            return NULL;
        }
    }

    //随机层数
    level = RandomLevel();
    //如果随机的层数比当前skiplist的层数要大，则补充高出的层的每层update节点信息
    if (level > mskiplist.level)
    {
        for (i = mskiplist.level; i < level; i++)
        {
            rank[i] = 0;
            update[i] = mskiplist.header; //当前节点初始化为header
            update[i]->levels[i].span = mskiplist.length; //span即skiplist的长度
        }
        mskiplist.level = level;  //更新skiplist的层数
    }

    //创建节点
    tmpNode = CreateSkipListNode(level,score,ele);
    for (i = 0; i < level; i++)
    {
        //修改创建节点tmpNode,需要修改的节点的forward指针(从每层来看上是一个链表的插入操作即把tmpNode插入到update[i]后面)
        tmpNode->levels[i].forward = update[i]->levels[i].forward;
        update[i]->levels[i].forward = tmpNode;

        //修改相关节点的span
        tmpNode->levels[i].span = update[i]->levels[i].span - (rank[0] - rank[i]);
        update[i]->levels[i].span = (rank[0] - rank[i]) + 1;
    }

    //如果随机的层数小于当前skiplist的层数,因为插入了一个新的节点，则把每层需要修改的节点的span加1
    for (i = level; i < mskiplist.level; i++)
    {
        update[i]->levels[i].span++;
    }

    //在0层中把新插入节点的前向指针指向修改的指针
    tmpNode->backward = (update[0] == mskiplist.header) ? NULL : update[0];
    //如果新插入节点的后一个节点存在，则把他的前向指针指向新插入的节点
    if (tmpNode->levels[0].forward)
    {
        tmpNode->levels[0].forward->backward = tmpNode;
    }
    else{//否则新插入的节点为tail
        mskiplist.tail = tmpNode;
    }
    mskiplist.length++;
    //更新map
    mrankMap[ele] = tmpNode;
    //如果长度大于最大长度
    if (mrankMaxLen > 0 && mskiplist.length > mrankMaxLen)
    {
        DeleteRangeByRank(mrankMaxLen + 1);
    }
    return tmpNode;
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
int CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::DeleteNode(MEMBER_TYPE ele) {
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *update[ZSKIPLIST_MAXLEVEL] = {0}; //每一层需要修改的结点,在每层中，新的节点需要插入在该节点的后面
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> * tmpNode;
    int i;

    rank_it it = mrankMap.find(ele);
    //如果节点已存在，先删除老的节点
    if(it != mrankMap.end())
    {
        SCORE_TYPE& score = it->second->score;
        tmpNode = mskiplist.header;
        for (i = mskiplist.level-1; i >= 0; i--) {
            //不可能出现这种情况，如果出现则说明调表结构已经被破坏,排行榜已经乱了
            if (!(i < tmpNode->level))
            {
                printf("The data of skiplist is bad\n");
                return 0;
            }
            //如果当前节点的下一个节点is not null，且他的下一个节点的score小于插入节点的score，或者他们积分相等但是ele小于插入节点的ele，则继续往后找，直到
            //找到条件不满足的跳出循环，则每层要删除的节点的前一个节点都存在update数组中
            while (tmpNode->levels[i].forward &&
                (tmpNode->levels[i].forward->score > score ||
                    (tmpNode->levels[i].forward->score == score && tmpNode->levels[i].forward->ele > ele)))
            {
                tmpNode = tmpNode->levels[i].forward;
            }
            update[i] = tmpNode;
        }

        tmpNode = tmpNode->levels[0].forward;
        //判断找到的节点是否是要删除的节点
        if (tmpNode && score == tmpNode->score && tmpNode->ele == ele) {
            DeleteNode(tmpNode, update);
            mrankMap.erase(ele);
            delete tmpNode;
            return 1;
        }
    }

    return 0; /* not found */
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
unsigned long CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::DeleteRangeByRank(unsigned long rank)
{
    return DeleteRangeByRank(rank,rank);
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
unsigned long CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::DeleteRangeByRank(unsigned long start, unsigned long end)
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *update[ZSKIPLIST_MAXLEVEL]; //每一层需要修改的结点,在每层中，新的节点需要插入在该节点的后面
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> * tmpNode;
    unsigned long traversed = 0, removed = 0;
    int i;

    tmpNode = mskiplist.header;
    for (i = mskiplist.level-1; i >= 0; i--) {
        //不可能出现这种情况，如果出现则说明调表结构已经被破坏,排行榜已经乱了
        if (!(i < tmpNode->level))
        {
            printf("The data of skiplist is bad\n");
            return 0;
        }
        while (tmpNode->levels[i].forward && (traversed + tmpNode->levels[i].span) < start) {
            traversed += tmpNode->levels[i].span;
            tmpNode = tmpNode->levels[i].forward;
        }
        update[i] = tmpNode;
    }

    traversed++;
    tmpNode = tmpNode->levels[0].forward;
    while (tmpNode && traversed <= end) {
        skiplistNode<SCORE_TYPE,MEMBER_TYPE> *next = tmpNode->levels[0].forward;
        DeleteNode(tmpNode,update);
        mrankMap.erase(tmpNode->ele);
        delete  tmpNode;
        traversed++;
        tmpNode = next;
        removed++;
    }
    return removed;
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
skiplistNode<SCORE_TYPE,MEMBER_TYPE> *CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::UpdateScore(SCORE_TYPE newscore,MEMBER_TYPE ele) {
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *update[ZSKIPLIST_MAXLEVEL] = {0}; //每一层需要修改的结点,在每层中，新的节点需要插入在该节点的后面
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> * tmpNode;
    int i;
    rank_it it = mrankMap.find(ele);
    //如果节点已存在，先删除老的节点
    if(it != mrankMap.end())
    {
        SCORE_TYPE& curscore = it->second->score;
        tmpNode = mskiplist.header;
        for (i = mskiplist.level-1; i >= 0; i--) {
            //不可能出现这种情况，如果出现则说明调表结构已经被破坏,排行榜已经乱了
            if (!(i < tmpNode->level))
            {
                printf("The data of skiplist is bad\n");
                return NULL;
            }
            //如果当前节点的下一个节点is not null，且他的下一个节点的score小于插入节点的score，或者他们积分相等但是ele小于插入节点的ele，则继续往后找，直到
            //找到条件不满足的跳出循环，则每层要update的节点的前一个节点都存在update数组中
            while (tmpNode->levels[i].forward &&
                    (tmpNode->levels[i].forward->score > curscore ||
                        (tmpNode->levels[i].forward->score == curscore && tmpNode->levels[i].forward->ele > ele)))
            {
                tmpNode = tmpNode->levels[i].forward;
            }
            update[i] = tmpNode;
        }

        tmpNode = tmpNode->levels[0].forward;
        //没找到???,不可能
        if (tmpNode == NULL || tmpNode->ele != it->second->ele || tmpNode->score != it->second->score)
        {
            printf("Update score failed,update node not found \n");
            return NULL;
        }
        //新的节点还在原有节点的中间，更新后的节点位置并不会发生变化，则直接更新积分
        if ((tmpNode->backward == NULL || tmpNode->backward->score > newscore) &&
            (tmpNode->levels[0].forward == NULL || tmpNode->levels[0].forward->score < newscore))
        {
            tmpNode->score = newscore;
            return tmpNode;
        }

        //删除老的节点
        DeleteNode(tmpNode, update);
        mrankMap.erase(tmpNode->ele);
        delete tmpNode;
        skiplistNode<SCORE_TYPE,MEMBER_TYPE> *newnode = InsertOrUpdateNode(newscore, tmpNode->ele);
        mrankMap[ele] = newnode;
        return newnode;
    }
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
unsigned long CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::Rank(MEMBER_TYPE ele)
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *tmpNode;
    unsigned long rank = 0;
    int i;

    rank_it it = mrankMap.find(ele);
    if (it == mrankMap.end())
    {
        return 0;
    }
    SCORE_TYPE score = it->second;
    tmpNode = mskiplist.header;
    for (i = mskiplist.level-1; i >= 0; i--) {
        //如果当前节点的下一个节点is not null，且他的下一个节点的score小于插入节点的score，或者他们积分相等但是ele小于等于插入节点的ele，则继续往后找，直到
        //找到条件不满足的跳出循环
        while (tmpNode->levels[i].forward &&
                 (tmpNode->levels[i].forward->score > score ||
                    (tmpNode->levels[i].forward->score == score && tmpNode->levels[i].forward->ele >= ele))) {
            rank += tmpNode->levels[i].span;  //span该层中当前节点与下一结点之间在level[0]中的结点数累计的结果即每一层要修改的结点的排名
            tmpNode = tmpNode->levels[i].forward;
        }

        if (tmpNode->ele && ele == tmpNode->ele) {
            return rank;
        }
    }
    return 0;
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
skiplistNode<SCORE_TYPE,MEMBER_TYPE> *CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::GetNodeByRank(unsigned long rank)
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *tmpNode;
    unsigned long traversed = 0;
    int i;

    tmpNode = mskiplist.header;
    for (i = mskiplist.level-1; i >= 0; i--) {
        while (tmpNode->levels[i].forward && (traversed + tmpNode->levels[i].span) <= rank)
        {
            traversed += tmpNode->levels[i].span;
            tmpNode = tmpNode->levels[i].forward;
        }
        if (traversed == rank) {
            return tmpNode;
        }
    }
    return NULL;
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
skiplistNode<SCORE_TYPE,MEMBER_TYPE>* CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::CreateSkipListNode(int level,SCORE_TYPE score,MEMBER_TYPE value)
{
    skiplistNode<SCORE_TYPE,MEMBER_TYPE> *zn = new skiplistNode<SCORE_TYPE,MEMBER_TYPE>(level);
    zn->score = score;
    zn->ele = value;
    return zn;
}

template <typename SCORE_TYPE,typename MEMBER_TYPE>
void CRankSkipList<SCORE_TYPE,MEMBER_TYPE>::DeleteNode(skiplistNode<SCORE_TYPE,MEMBER_TYPE> *deleteNode, skiplistNode<SCORE_TYPE,MEMBER_TYPE> **update)
{
    int i;
    for (i = 0; i < mskiplist.level; i++) {
        //如果需要修改的节点的后一个节点是要删除的节点,
        if (update[i]->levels[i].forward == deleteNode) {
            //假设没有删除，此节点到下一节点(deleteNode)的下一个节点的的span = 此节点的span + 删除节点的span,
            //此节点的下一节点删除后，此节点的span等于此节点的span加上删除节点的span - 1
            update[i]->levels[i].span += deleteNode->levels[i].span - 1;
            //赋值后向指针,在每层上看，就是一个链表删除一个节点的操作
            update[i]->levels[i].forward = deleteNode->levels[i].forward;
        }
        else {
            //如果需要修改的节点的后一个节点不是要删除的节点,则说明删除节点的层数小于该层，该层链表之上没有要删除的节点，直接span - 1即可
            update[i]->levels[i].span -= 1;
        }
    }

    //修改地0层的前向指针
    if (deleteNode->levels[0].forward) {
        deleteNode->levels[0].forward->backward = deleteNode->backward;
    } else {
        mskiplist.tail = deleteNode->backward;
    }

    //删除节点后，遍历每层，如果该层之后头头节点，则删除该层
    while(mskiplist.level > 1 && mskiplist.header->levels[mskiplist.level-1].forward == NULL)
        mskiplist.level--;
    mskiplist.length--;
}
#endif //SERVER_SKIP_LIST_H
