// kfol_core.cpp - KFOL 爬塔战斗核心 (CPU) — 忠实移植 kfol2 算法
// 覆盖: 原版 calcBattleStart 属性链 / 四分支期望战斗 / enemyBoost / 加权爬塔 / INIT_WEIGHT+多步爬山搜索
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
#define MAX_ROUND 200

// ---- 属性/stat 索引 (原版枚举) ----
enum { STR=0, VIT=1, AGI=2, DEX=3, INT=4, RES=5 };
enum { ATK=0, LFE=1, SPD=2, CRT=3, TEC=4, MAG=5, PRES=6, DEF=6, ACR=8, ASR=9, LCH=10, HP=11 };
enum { NORM=0, STRG=1, TOGH=2, FAST=3, CLVR=4, BOSS=5, CLVR3=6 };

extern "C" {
int kfolEnemyRate[ENEMY_NUM] = {60, 10, 10, 10, 10, 100};
int kfolAura = 501;
int kfolCoef = 11;

// 道具 (kfol.in 顺序: 蕾米 十六夜 钥匙 CD 药 券)
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

// -------- 敌人基础属性 (原版 getEnemyBaseAttr: 7 段系数) --------
static int kfolEnemyBase(int lvl, int attr)
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

// -------- 敌人类型强化 (原版 enemyBoost: num/den 精确倍率) --------
static const int kEnemyBoost[ENEMY_NUM][ATTR_NUM][2] = {
    {{1,1}, {1,1}, {1,1}, {1,1}, {1,1}, {1,1}},
    {{3,1}, {3,2}, {1,1}, {3,10}, {3,10}, {3,10}},
    {{1,1}, {3,1}, {3,10}, {3,10}, {3,10}, {3,2}},
    {{3,10},{3,10},{5,1}, {3,1}, {3,10}, {3,10}},
    {{3,10},{3,10},{7,10},{3,10},{6,1}, {3,10}},
    {{3,2}, {2,1}, {3,2}, {6,5}, {6,5}, {6,5}}
};

// -------- 玩家 12 维 stat: 原版 calcBattleStart 属性链 --------
// attr 为加点后的裸六维; 函数内部完成 道具加成->光环->装备强化->CRT/TEC 公式->12维
void kfolCalcPlayerStats(const int* attr, int wpnLvl, int amrLvl, int* out)
{
    memset(out, 0, sizeof(int) * STAT_NUM);
    int aura = kfolAura;

    // 1) 道具基础属性加成 (作用于裸属性, 再吃光环)
    int a[ATTR_NUM];
    for (int i = 0; i < ATTR_NUM; ++i) a[i] = attr[i];
    a[STR] += gItems[0] + gItems[4] * 5;   // 蕾米漫画 +1力/本, 药 +5力/瓶
    a[VIT] += gItems[0] + gItems[4] * 5;
    a[AGI] += gItems[1] + gItems[4] * 5;   // 十六夜漫画 +1敏/本
    a[DEX] += gItems[1] + gItems[4] * 5;
    a[INT] += gItems[4] * 5;
    a[RES] += gItems[4] * 5;

    // 2) 光环: newAttr = base + base*aura/1000
    int pNew[ATTR_NUM];
    int attrSum = 0;
    for (int i = 0; i < ATTR_NUM; ++i)
    {
        pNew[i] = a[i] + a[i] * aura / 1000;
        attrSum += pNew[i];
    }

    // 3) 装备强化系数 (无装备文本从属性时 enhance=0 → 属性值=系数*等级)
    //    wpnLvl/amrLvl 由 UI 输入; 装备文本解析的 sub 属性经 JNI 扩展后在此接入
    int wpnVal[6] = {0}, amrVal[6] = {0};
    static const int kWC[6] = {5, 2, 1, 1, 1, 3};      // 武器: ATK SPD CRT SKL BRC LCH
    static const int kAC[6] = {5, 20, 1, 1, 10, 10};   // 防具: HEL SLD AMR RFL CRD SRD
    static const int kAB[6] = {100, 500, 0, 150, 0, 0};
    for (int i = 0; i < 6; ++i)
    {
        wpnVal[i] = (int64_t)kWC[i] * wpnLvl * (1000 + 0) / 1000;
        amrVal[i] = (int64_t)kAC[i] * amrLvl * (1000 + 0) / 1000 + kAB[i];
    }

    // 4) 12 维 stat (原版公式, 武器默认拳套)
    out[ATK] = pNew[STR] * 5 + wpnVal[0];                       // ATK = STR*5 + 装备ATK
    out[LFE] = pNew[VIT] * 20;                                  // LFE = VIT*20 (拳套)
    out[SPD] = pNew[AGI] * 2 + wpnVal[1];                       // SPD = AGI*2 + 装备SPD
    out[CRT] = (pNew[DEX] * 201 + 100) / (pNew[DEX] * 2 + 200) + wpnVal[2];
    if (out[CRT] > 99) out[CRT] = 99;
    out[TEC] = (pNew[INT] * 201 + 90) / (pNew[INT] * 2 + 180) + wpnVal[3];
    if (out[TEC] > 99) out[TEC] = 99;
    out[MAG] = (pNew[VIT] + pNew[INT]) * 4 + wpnVal[0];         // 拳套 MAG
    out[PRES] = pNew[RES];                                      // 意志 → 防御基础
    out[ACR] = 200 + wpnVal[2];                                 // 暴击倍率基数
    out[ASR] = 100 + wpnVal[3];                                 // 技能倍率基数
    out[LCH] = (int64_t)wpnVal[4] * 50000 / 50000;              // 吸血
    out[HP] = out[LFE] + 100;
    if (gItems[0] >= 50) out[LFE] += 700;                       // 满50蕾米 +700生命 (加在最大生命)
    if (gItems[1] >= 50) out[SPD] += 100;                       // 满50十六夜 +100攻速
    if (!(gItems[0] >= 50)) out[HP] = out[LFE] + 100;
    if (gItems[0] >= 50) out[HP] = out[LFE] + 100;
    // CD/抗性等由 kfolBattle 用 eStat 处理
}

// -------- 敌人属性: 基础六维 + enemyBoost + CD 减益 (原版 calcBattleStart 敌人侧) --------
void kfolCalcEnemyStats(int lvl, int type, int* out)
{
    memset(out, 0, sizeof(int) * STAT_NUM);
    int base[ATTR_NUM];
    for (int i = 0; i < ATTR_NUM; ++i)
        base[i] = kfolEnemyBase(lvl, i) * kEnemyBoost[type][i][0] / kEnemyBoost[type][i][1];
    // 原版敌人 stat 公式
    out[ATK] = base[STR] * 3;                                    // ATK = STR*3
    out[LFE] = base[VIT] * 20;                                   // LFE = VIT*20
    out[SPD] = base[AGI] * 2;                                    // SPD = AGI*2
    out[CRT] = (base[DEX] * 201 + 100) / (base[DEX] * 2 + 200);
    if (out[CRT] > 99) out[CRT] = 99;
    out[TEC] = (base[INT] * 201 + 90) / (base[INT] * 2 + 180);
    if (out[TEC] > 99) out[TEC] = 99;
    out[MAG] = type == STRG ? base[STR] * 3 : type == CLVR ? base[INT] * 15 : 0;
    out[PRES] = base[RES];
    out[ACR] = 200;
    out[ASR] = 100;
    out[HP] = out[LFE];
    // CD 道具: 每张降敌生命上限0.8%, 满30 追加降攻击10%
    if (gItems[3] > 0)
    {
        out[HP] = ((int64_t)out[HP] * (250 - gItems[3] * 2) + 125) / 250;
        if (out[HP] < 1) out[HP] = 1;
    }
    if (gItems[3] >= 30) out[ATK] = (out[ATK] * 9 + 5) / 10;
}

// 敌人属性应用 CD 道具效果 (兼容旧接口)
void kfolApplyItemDebuff(int* eStat)
{
    if (gItems[3] > 0)
    {
        eStat[HP] = eStat[HP] * (1000 - gItems[3] * 8) / 1000;
        if (eStat[HP] < 1) eStat[HP] = 1;
    }
    if (gItems[3] >= 30) eStat[ATK] = eStat[ATK] * 9 / 10;
}

// -------- 单场战斗: 原版四分支期望 (TEC 技能/命中 × CRT 暴击, 减伤, 吸血) --------
// 返回剩余 HP (>0 胜, <=0 败; 回合耗尽视败)
int kfolBattle(const int* pStat, const int* eStat)
{
    int pHp = pStat[HP];
    int eHp = eStat[HP];
    int rounds = 0;
    while (eHp > 0 && rounds < MAX_ROUND)
    {
        rounds++;
        // 防守方减伤 (原版 def 公式应用到 10000 基准)
        int pDef = pStat[PRES] >= 0 ? ((int64_t)pStat[PRES] * 20001 + 150) / (pStat[PRES] * 2 + 300) : 0;
        if (pDef > 9900) pDef = 9900;
        // 玩家攻击: 四分支期望伤害 (原版 calcBattle1 的攻击分支)
        int tec = pStat[TEC] > 99 ? 99 : pStat[TEC];
        int crt = pStat[CRT] > 99 ? 99 : pStat[CRT];
        // 期望伤害: 非技能/非暴击 (100-tec)(100-crt) + 技能×非暴 + 暴击×非技 + 技能+暴击
        int64_t dmg = 0;
        int p0 = (100 - tec) * (100 - crt);   // 普通
        int p1 = (tec) * (100 - crt);         // 技能
        int p2 = (100 - tec) * (crt);         // 暴击
        int p3 = (tec) * (crt);               // 技能+暴击
        // 原版 dmg0: 普通=ATK*100; 暴击=ATK*ACR; 技能 +MAG*100; (万分率)
        dmg = (int64_t)pStat[ATK] * 100 * p0 +
              ((int64_t)pStat[ATK] * 100 + (int64_t)pStat[MAG] * 100) * p1 +
              (int64_t)pStat[ATK] * pStat[ACR] * p2 +
              ((int64_t)pStat[ATK] * pStat[ACR] + (int64_t)pStat[MAG] * 100) * p3;
        dmg = dmg / 10000;                    // 归一化概率
        // 减伤 (原版: (10000 - eDEF) 万分率)
        int eDef = eStat[PRES] >= 0 ? ((int64_t)eStat[PRES] * 20001 + 150) / (eStat[PRES] * 2 + 300) : 0;
        if (eDef > 9900) eDef = 9900;
        dmg = dmg * (10000 - eDef) / 10000 + pStat[LCH];
        if (dmg < 1) dmg = 1;
        eHp -= (int)dmg;
        if (eHp <= 0) break;
        // 敌人攻击 (含暴击期望)
        int eTec = eStat[TEC] > 99 ? 99 : eStat[TEC];
        int eCrt = eStat[CRT] > 99 ? 99 : eStat[CRT];
        int64_t edmg = (int64_t)eStat[ATK] * 100 * ((100 - eTec) * (100 - eCrt) +
                       (100 - eTec) * eCrt * 2) / 10000;
        edmg = edmg * (10000 - pDef) / 10000;
        if (edmg < 1) edmg = 1;
        pHp -= (int)edmg;
        if (pHp <= 0) return pHp;
    }
    return eHp <= 0 ? pHp : 0;
}

// -------- 爬塔: 从 startLvl 逐层, 按 enemyRate 加权的期望剩余 HP --------
int kfolClimb(int startLvl, int maxLvl, const int* attr, int wpnLvl, int amrLvl)
{
    int pStat[STAT_NUM];
    kfolCalcPlayerStats(attr, wpnLvl, amrLvl, pStat);
    int hp = pStat[HP];
    int lvl = startLvl;
    while (lvl <= maxLvl)
    {
        // 原版: 普通层出现 NORM..CLVR (按出现率), 10 的倍数层 BOSS
        int worstHp = -1;
        for (int e = 0; e < ENEMY_NUM; ++e)
        {
            int eStat[STAT_NUM];
            kfolCalcEnemyStats(lvl, e, eStat);
            int remain = kfolBattle(pStat, eStat);
            if (remain > worstHp) worstHp = remain;
        }
        if (worstHp <= 0) break;
        hp = worstHp;
        pStat[HP] = hp;
        lvl++;
    }
    return lvl - 1;
}

// -------- 加点搜索: 原版 INIT_WEIGHT 权重起点 + 多步爬山 (searchBestAttr) --------
static const int INIT_WEIGHT[][ATTR_NUM] = {
    {0, 0, 1, 0, 0, 0}, {1, 1, 1, 1, 1, 1}, {1, 1, 1, 0, 0, 0},
    {1, 0, 4, 2, 0, 0}, {1, 0, 1, 0, 4, 0}, {0, 0, 2, 0, 4, 2},
    {1, 1, 3, 3, 3, 0}, {3, 1, 3, 0, 0, 3}
};
static const int INIT_PATTERN_NUM = 8;

static void kfolInitAttr(int pattern, int points, int* a)
{
    int weightSum = 0;
    for (int i = 0; i < ATTR_NUM; ++i) weightSum += INIT_WEIGHT[pattern][i];
    int basePoints = points - ATTR_NUM;
    for (int i = 0; i < ATTR_NUM; ++i)
    {
        a[i] = basePoints * INIT_WEIGHT[pattern][i] / weightSum + 1;
        if (a[i] < 1) a[i] = 1;
        points -= a[i];
    }
    if (points > 0)
    {
        bool incWeightedOnly = true;
        while (points)
        {
            bool changed = false;
            for (int i = 0; i < ATTR_NUM; ++i)
            {
                if (points && (!incWeightedOnly || INIT_WEIGHT[pattern][i] > 0))
                { ++a[i]; --points; changed = true; }
            }
            if (!changed) incWeightedOnly = false;
        }
    }
}

// 综合塔怪难度+装备强度的极限搜索: 8 模式起点 × 多步爬山 × 战斗模拟评估
void kfolSearchAttrs(int points, int startLvl, int maxLvl, int wpnLvl, int amrLvl,
                     int aura, const int* items, int* bestAttr, int* bestLvl)
{
    kfolAura = aura;
    kfolSetItems(items);
    int bestLvlSoFar = startLvl - 1;

    for (int pattern = 0; pattern < INIT_PATTERN_NUM; ++pattern)
    {
        int attr[ATTR_NUM];
        kfolInitAttr(pattern, points, attr);
        int curLvl = kfolClimb(startLvl, maxLvl, attr, wpnLvl, amrLvl);
        if (curLvl > bestLvlSoFar)
        {
            bestLvlSoFar = curLvl;
            for (int i = 0; i < ATTR_NUM; ++i) bestAttr[i] = attr[i];
        }
        const int steps[] = {10, 5, 2, 1};
        for (size_t si = 0; si < sizeof(steps) / sizeof(steps[0]); ++si)
        {
            int step = steps[si];
            bool improved = true;
            for (int iter = 0; iter < 8 && improved; ++iter)
            {
                improved = false;
                int bi = -1, bj = -1, bestDeltaLvl = curLvl;
                for (int i = 0; i < ATTR_NUM; ++i)
                {
                    if (attr[i] - step < 1) continue;
                    attr[i] -= step;
                    for (int j = 0; j < ATTR_NUM; ++j)
                    {
                        if (j == i) continue;
                        attr[j] += step;
                        int l = kfolClimb(startLvl, maxLvl, attr, wpnLvl, amrLvl);
                        if (l > bestDeltaLvl) { bestDeltaLvl = l; bi = i; bj = j; }
                        attr[j] -= step;
                    }
                    attr[i] += step;
                }
                if (bi != -1 && bestDeltaLvl > curLvl)
                {
                    attr[bi] -= step;
                    attr[bj] += step;
                    curLvl = bestDeltaLvl;
                    improved = true;
                    if (curLvl > bestLvlSoFar)
                    {
                        bestLvlSoFar = curLvl;
                        for (int k = 0; k < ATTR_NUM; ++k) bestAttr[k] = attr[k];
                    }
                }
            }
        }
    }
    *bestLvl = bestLvlSoFar;
}

} // extern "C"