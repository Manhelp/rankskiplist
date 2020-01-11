//
// Created by dguco on 20-1-11.
//

#include <map>
#include "rankskip_list.h"
class Score
{
public:
    Score()
    {
        id = -1;
        chinese = 0;
        math = 0;
        english = 0;
    }
    Score(int id,int chinese, int math, int english)
        : id (id),chinese(chinese), math(math), english(english)
    {

    }
    int Sum()
    {
        return chinese + math + english;
    }

    bool operator ==(const Score& d)
    {
        return this->id == d.id && this->chinese == d.chinese &&
            this->math == d.math && this->english == d.english;
    }

    bool operator > (const Score& d)
    {
        int sumMe = chinese + math + english;
        int sumHe = d.chinese + d.math + d.english;
        if (sumMe < sumHe)
        {
            return true;
        }
        else if (sumMe == sumHe)
        {
            if (this->chinese < d.chinese)
            {
                return true;
            }
            else if (this->chinese == d.chinese)
            {
                if (this->math < d.math)
                {
                    return true;
                }
                else if (this->math == d.math)
                {
                    return id < d.id;
                }
            }
        }

        return false;
    }

    bool operator < (const Score& d)
    {
        return !(*this > d);
    }

    bool operator <= (const Score& d)
    {
        return (*this < d) || (*this == d);
    }

    int getId() const
    {
        return id;
    }
    int getChinese() const
    {
        return chinese;
    }
    int getMath() const
    {
        return math;
    }

    int getEnglish() const
    {
        return english;
    }

private:
    int id;
    int chinese;
    int math;
    int english;
};

int main()
{
    CRankSkipList<Score,int>* rankSkipList = new CRankSkipList<Score,int>();
    rankSkipList->InsertOrUpdateNode(Score(1,100,99,100),1);
    rankSkipList->InsertOrUpdateNode(Score(2,99,99,99),2);
    rankSkipList->InsertOrUpdateNode(Score(3,10,29,39),3);
    rankSkipList->InsertOrUpdateNode(Score(4,30,20,10),4);
    rankSkipList->InsertOrUpdateNode(Score(5,99,99,99),5);
    rankSkipList->InsertOrUpdateNode(Score(6,100,98,98),6);
    rankSkipList->InsertOrUpdateNode(Score(7,99,99,100),7);
    rankSkipList->InsertOrUpdateNode(Score(8,99,99,99),8);
    rankSkipList->InsertOrUpdateNode(Score(9,90,90,90),9);
    rankSkipList->InsertOrUpdateNode(Score(10,90,90,90),10);
    rankSkipList->InsertOrUpdateNode(Score(11,80,85,90),11);
    rankSkipList->InsertOrUpdateNode(Score(12,90,80,85),12);
    rankSkipList->InsertOrUpdateNode(Score(13,80,85,90),13);
    rankSkipList->InsertOrUpdateNode(Score(14,80,90,85),14);
    for (int i = 15;i <= 100;i++)
    {
        rankSkipList->InsertOrUpdateNode(Score(i,std::rand() % 100 + 1,std::rand() % 100 + 1,std::rand() % 100 + 1),i);
    }
//    skiplistNode<Score,int>* node = rankSkipList->GetNodeByRank(1);
//    int i = 1;
//    while (node != NULL)
//    {
//        Score score = node->score;
//        printf("rank = %d,id = %d,sum = %d,chinese = %d,math = %d,english = %d\n",
//               i,score.getId(),score.Sum(), score.getChinese(),score.getMath(),score.getEnglish());
//        i++;
//        node = node->levels[0].forward;
//    }
    for (int i = 1;i <= 100;i++)
    {
        skiplistNode<Score,int>* node = rankSkipList->GetNodeByRank(i);
        if (node != NULL)
        {
            Score score = node->score;
            printf("rank = %d,id = %d,sum = %d,chinese = %d,math = %d,english = %d\n",
            i,score.getId(),score.Sum(), score.getChinese(),score.getMath(),score.getEnglish());
        }
    }
}