// kfol_core.cpp - KFOL 爬塔战斗核心 (CPU)
// 属性公式/NPC属性/战斗模拟/层数推进, 由 CPU 执行精确计算与调度
// GPU 负责大规模蒙特卡洛样本, 本文件负责确定性模拟与搜索
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <algorithm>
using namespace std;

#define ATTR_NUM 6   // STR VIT AGI DEX INT RES
#define STAT_NUM 12
#define ENEMY_NUM 6  // NORM STRG TOGH FAST CLVR BOSS
#define MAX_LVL 240

// ---- 属性 ----
enum { STR=0, VIT=1, AGI=2, DEX=3, INT=4, RES=5 };
enum { ATK=0, LFE=1, SPD=2, CRT=3, TEC=4, MAG=5, DEF=6, ACR=8, ASR=9, LCH=10, HP=11 };
enum { NORM=0, STRG=1, TOGH=2, FAST=3, CLVR=4, BOSS=5 };

extern "C" {
int kfolEnemyRate[ENEMY_NUM] = {10, 10, 10, 10, 10, 10};
int kfolAura = 501;
int kfolCoef = 11;

// 敌人基础属性: 层数 x 系数 (与 readme 一致)
int kfolEnemyBase(int lvl, int attr)
{
    static const int coef[7][6] = {
        {5,5,2,2,2,1}, {10,7,3,3,3,4}, {13,10,5,6,6,8},
        {30,30,20,20,20,20}, {55,55,45,45,45,45},
        {75,75,55,55,55,55}, {99,99,99,99,99,99}
    };
    int band = lvl <= 50 ? 0 : lvl <= 100 ? 1 : lvl <= 200 ? 2 :
               lvl <= 210 ? 3 : lvl <= 220 ? 4 : lvl <= 230 ? 5 : 6;
    return lvl * coef[band][attr];
}

// 道具效果 (kfol.in 顺序: 蕾米漫画 十六夜漫画 钥匙 CD 药 券)
// 蕾米: 每本+1力量+1体质, 满50 +700生命
// 十六夜: 每本+1敏捷+1灵活, 满50 +100攻击速度
// 钥匙: 满30 +30可分配点 (调用方已加)
// CD: 每张降对手生命上限0.8%, 满30 追加降对手10%攻击
// 药: 每瓶全属性+5(不含耐力幸运), 满10 +120可分配点 (调用方已加)
static int gItems[6] = {0};
static int gHpHeal = 8, gHpStep = 100;

void kfolSetItems(const int* items)
{
    for (int i = 0; i < 6; ++i) gItems[i] = items[i];
}

void kfolSetHpParams(int heal, int step)
{
    gHpHeal = heal > 0 ? heal : 8;
    gHpStep = step > 0 ? step : 100;
}

// 玩家战斗属性: 基础属性 + 装备 + 光环 + 道具
void kfolCalcPlayerStats(const int* attr, int wpnLvl, int amrLvl, int* out) // out[12]
{
    memset(out, 0, sizeof(int) * STAT_NUM);
    int aura = kfolAura; // 千分率

    // 道具直接属性加成 (作用于基础属性, 再吃光环)
    int bAttr[ATTR_NUM];
    for (int i = 0; i < ATTR_NUM; ++i) bAttr[i] = attr[i];
    bAttr[STR] += gItems[0] + gItems[4] * 5;      // 漫画+药
    bAttr[VIT] += gItems[0] + gItems[4] * 5;
    bAttr[AGI] += gItems[1] + gItems[4] * 5;
    bAttr[DEX] += gItems[1] + gItems[4] * 5;
    bAttr[INT] += gItems[4] * 5;
    bAttr[RES] += gItems[4] * 5;

    out[ATK] = bAttr[STR] * (1000 + aura) / 1000;
    out[LFE] = bAttr[VIT] * (1000 + aura) / 1000;
    out[SPD] = bAttr[AGI] * (1000 + aura) / 1000;
    out[CRT] = bAttr[DEX] * (1000 + aura) / 1000;
    out[TEC] = bAttr[INT] * (1000 + aura) / 1000;
    out[MAG] = bAttr[RES] * (1000 + aura) / 1000;
    // 装备等级加成 (简化: 每级 +2 主属性)
    out[ATK] += wpnLvl * 2;
    out[SPD] += wpnLvl;
    out[LFE] += amrLvl * 3;
    out[DEF] = amrLvl * 4;
    // 道具满额加成
    if (gItems[0] >= 50) out[LFE] += 700;
    if (gItems[1] >= 50) out[SPD] += 100;
    out[HP] = out[LFE] * 10 + 100;
    out[ACR] = 100;
    out[ASR] = 100;
    out[LCH] = 0;
}

// 敌人属性应用 CD 道具效果: 每张降生命0.8%, 满30 降攻击10%
void kfolApplyItemDebuff(int* eStat)
{
    if (gItems[3] > 0)
    {
        // 降低对手生命值上限
        eStat[HP] = eStat[HP] * (1000 - gItems[3] * 8) / 1000;
        if (eStat[HP] < 1) eStat[HP] = 1;
    }
    if (gItems[3] >= 30)
    {
        eStat[ATK] = eStat[ATK] * 9 / 10;
    }
}

void kfolCalcEnemyStats(int lvl, int type, int* out) // out[12]
{
    memset(out, 0, sizeof(int) * STAT_NUM);
    // 基础六维
    int base[6];
    for (int i = 0; i < ATTR_NUM; ++i) base[i] = kfolEnemyBase(lvl, i);
    // 类型强化 (简化: 百分比)
    const int boost[ENEMY_NUM][6] = {
        {100,100,100,100,100,100}, {300,300,100,100,100,100},
        {100,300,100,100,100,100}, {100,100,500,300,300,100},
        {100,100,700,100,600,100}, {300,200,300,600,600,600}
    };
    for (int i = 0; i < ATTR_NUM; ++i)
        base[i] = base[i] * boost[type][i] / 100;
    out[ATK] = base[STR] * 5;
    out[LFE] = base[VIT] * 5;
    out[SPD] = base[AGI] * 3;
    out[CRT] = base[DEX] * 2;
    out[TEC] = base[INT] * 2;
    out[MAG] = base[RES];
    out[DEF] = 0;
    out[HP] = out[LFE] * 10;
}

// 单场确定性战斗: 返回剩余HP (>0 胜利, <=0 失败; 回合耗尽视为失败)
int kfolBattle(const int* pStat, const int* eStat)
{
    int pHp = pStat[HP];
    int eHp = eStat[HP];
    int rounds = 0;
    while (eHp > 0 && rounds < 200)
    {
        rounds++;
        // 原版: dmg0 = ATK*100 (非暴击) -> dmg = dmg0*(10000-DEF)/1000000
        // 等价: ATK*(10000-DEF)/10000
        int dmg = pStat[ATK] * (10000 - eStat[DEF]) / 10000 + pStat[LCH];
        if (dmg < 1) dmg = 1;
        eHp -= dmg;
        if (eHp <= 0) break;
        int edmg = eStat[ATK] * (10000 - pStat[DEF]) / 10000;
        if (edmg < 1) edmg = 1;
        pHp -= edmg;
        if (pHp <= 0) return pHp;
    }
    // 回合耗尽: 若敌人没死视为打不赢(平局), 返回 0 保证 climb 停止
    return eHp <= 0 ? pHp : 0;
}

// 爬塔: 从 startLvl 逐层挑战, 返回能通过的层数
// 每层按 6 种敌人出现率加权, 若当前 HP 不足以打赢则停止
int kfolClimb(int startLvl, int maxLvl, const int* attr, int wpnLvl, int amrLvl)
{
    int pStat[STAT_NUM];
    kfolCalcPlayerStats(attr, wpnLvl, amrLvl, pStat);
    int hp = pStat[HP];
    int lvl = startLvl;
    while (lvl <= maxLvl)
    {
        // 检查当前层所有敌人类型中最强的一个是否可胜
        int worstHp = -1;
        for (int e = 0; e < ENEMY_NUM; ++e)
        {
            int eStat[STAT_NUM];
            kfolCalcEnemyStats(lvl, e, eStat);
            kfolApplyItemDebuff(eStat);
            int remain = kfolBattle(pStat, eStat);
            if (remain > worstHp) worstHp = remain;
        }
        if (worstHp <= 0) break;  // 连最强敌人都打不过
        hp = worstHp;
        pStat[HP] = hp;
        lvl++;
    }
    return lvl - 1;  // 通过的层数
}

// 自动加点搜索: 简单爬山 - 从平均分配开始, 逐个加点选择收益最大属性
// 返回最优六维属性分配 (总点数 points, 每维至少 1)
void kfolSearchAttrs(int points, int startLvl, int maxLvl, int wpnLvl, int amrLvl,
                     int aura, const int* items, int* bestAttr, int* bestLvl)
{
    kfolAura = aura;
    kfolSetItems(items);
    int attr[ATTR_NUM];
    // 平均分配起点
    int base = points / ATTR_NUM;
    for (int i = 0; i < ATTR_NUM; ++i) attr[i] = base;
    int used = base * ATTR_NUM;
    attr[0] += points - used;  // 余数给力量

    int curLvl = kfolClimb(startLvl, maxLvl, attr, wpnLvl, amrLvl);
    *bestLvl = curLvl;
    for (int i = 0; i < ATTR_NUM; ++i) bestAttr[i] = attr[i];

    // 贪心: 若干轮, 每次微调 5% 点数测试增益
    for (int iter = 0; iter < 40; ++iter)
    {
        bool improved = false;
        for (int i = 0; i < ATTR_NUM; ++i)
        {
            for (int j = 0; j < ATTR_NUM; ++j)
            {
                if (i == j || attr[i] <= 1) continue;
                int test[ATTR_NUM];
                for (int k = 0; k < ATTR_NUM; ++k) test[k] = attr[k];
                test[i] -= 5; test[j] += 5;
                if (test[i] < 1) continue;
                int l = kfolClimb(startLvl, maxLvl, test, wpnLvl, amrLvl);
                if (l > curLvl)
                {
                    curLvl = l;
                    for (int k = 0; k < ATTR_NUM; ++k) attr[k] = test[k];
                    improved = true;
                }
            }
        }
        if (!improved) break;
    }
    *bestLvl = curLvl;
    for (int i = 0; i < ATTR_NUM; ++i) bestAttr[i] = attr[i];
}
} // extern "C"