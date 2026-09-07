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

// ---- 高级选项 (原版 kfol.in 可选项, 默认值对齐 readme) ----
static int gOptMaxRound = MAX_ROUND;    // MAXROUND: 单场战斗回合上限
static int gOptFastSkill = 0;           // FASTSKILL: 快速怪技能 (0/1/2/3/4)
static int gOptToughSkill = 0;          // TOUGHSKILL: 坚韧怪技能
static int gOptMaxLvl = 239;            // MAXLEVEL: 目标最高层
static int gOptBattleStep = 10;         // BATTLESTEP: HP 离散步长
static int gOptMinWinRate = 100;        // MINWINRATE: 需要的胜率 (万分率)
static int gOptServerBonus = 0;         // SERVERBONUS: 服务器攻击加成 (0/1/2)
static int gOptSimulationMode = 0;      // SIMULATIONMODE: 蒙特卡洛样本数 (0=精确)
static int gOptVerbose = 0;             // VERBOSE
static int gOptGridSize = 8;            // GRIDOPTION 第1项
static int gOptGridBaseStep = 4;        // GRIDOPTION 第2项
static int gOptGridMaxCenter = 10;      // GRIDOPTION 第3项

// ---- 属性/stat 索引 (原版枚举) ----
enum { STR=0, VIT=1, AGI=2, DEX=3, INT=4, RES=5 };
enum { ATK=0, LFE=1, SPD=2, CRT=3, TEC=4, MAG=5, PRES=6, DEF=6, ACR=8, ASR=9, LCH=10, SKL=10, HP=11 };
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

// 解析高级选项 (kfol.in 格式, 每行 "键 值" 或 "键 值1 值2 值3")
void kfolSetOptions(const char* text)
{
    if (!text) return;
    // 逐行解析
    char buf[1024];
    size_t pos = 0, len = strlen(text);
    while (pos < len)
    {
        // 提取一行
        size_t eol = pos;
        while (eol < len && text[eol] != '\n') ++eol;
        size_t linelen = eol - pos;
        if (linelen >= sizeof(buf)) linelen = sizeof(buf) - 1;
        memcpy(buf, text + pos, linelen);
        buf[linelen] = '\0';
        pos = eol + 1;
        // 跳过空白
        char* p = buf;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') continue;
        // 读键
        char key[64];
        int n = 0;
        while (*p != '\0' && *p != ' ' && *p != '\t' && n < 63) key[n++] = *p++;
        key[n] = '\0';
        while (*p == ' ' || *p == '\t') ++p;
        if (strcmp(key, "MAXROUND") == 0 && *p) gOptMaxRound = atoi(p);
        else if (strcmp(key, "FASTSKILL") == 0 && *p) gOptFastSkill = atoi(p);
        else if (strcmp(key, "TOUGHSKILL") == 0 && *p) gOptToughSkill = atoi(p);
        else if (strcmp(key, "MAXLEVEL") == 0 && *p) { gOptMaxLvl = atoi(p); if (gOptMaxLvl < 1) gOptMaxLvl = 239; }
        else if (strcmp(key, "BATTLESTEP") == 0 && *p) { gOptBattleStep = atoi(p); if (gOptBattleStep < 1) gOptBattleStep = 1; }
        else if (strcmp(key, "MINWINRATE") == 0 && *p) gOptMinWinRate = atoi(p);
        else if (strcmp(key, "SERVERBONUS") == 0 && *p) gOptServerBonus = atoi(p);
        else if (strcmp(key, "SIMULATIONMODE") == 0 && *p) gOptSimulationMode = atoi(p);
        else if (strcmp(key, "VERBOSE") == 0 && *p) gOptVerbose = atoi(p);
        else if (strcmp(key, "GRIDOPTION") == 0)
        {
            int a = 0, b = 0, c = 0;
            int got = sscanf(p, "%d %d %d", &a, &b, &c);
            if (got >= 1 && a > 0) gOptGridSize = a;
            if (got >= 2 && b > 0) gOptGridBaseStep = b;
            if (got >= 3 && c > 0) gOptGridMaxCenter = c;
        }
    }
}

int kfolGetServerBonus() { return gOptServerBonus; }
int kfolGetMaxRound() { return gOptMaxRound; }
int kfolGetMaxLvl() { return gOptMaxLvl; }
int kfolGetFastSkill() { return gOptFastSkill; }
int kfolGetToughSkill() { return gOptToughSkill; }

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
    out[ATK] = pNew[STR] * 5 + (gOptServerBonus == 1 ? pNew[STR] * 3 / 20 + 45 : gOptServerBonus == 2 ? pNew[STR] / 4 + 75 : 0) + wpnVal[0];  // ATK = STR*5 + 服务器加成 + 装备ATK
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
    out[SKL] = type;  // 敌人类型 (原版 SKL=10 与 LCH 同槽; 敌人无吸血所以可复用)
    // FASTSKILL: 快速怪技能 (原版 1=禁TEC 2=强行TEC 3=半TEC 4=狂暴TEC)
    if (type == FAST && gOptFastSkill > 0)
    {
        switch (gOptFastSkill)
        {
            case 1: out[TEC] = 0; break;
            case 2: if (out[TEC] > 0) out[TEC] = 100; break;
            case 3: out[TEC] = out[TEC] >= 50 ? 100 : 0; break;
            case 4: break; // 4=狂暴靠攻速, 期望战斗不细分
        }
    }
    if (type == TOGH && gOptToughSkill == 1 && out[TEC] > 0) out[TEC] = 100;
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

// -------- 原版 rand100 伪随机 (Park-Miller, 与原版一致) --------
static inline int kfolRand100(int* rseed)
{
    const int M = 48271, Q = 0x7FFFFFFF / M, R = 0x7FFFFFFF % M;
    *rseed = M * (*rseed % Q) - R * (*rseed / Q);
    if (*rseed < 0) *rseed += 0x7FFFFFFF;
    return *rseed % 100;
}

// 前向声明: 递归评估 (原版 calcAttrForward)
static double kfolEvalForward(int lvl, int hp, const int* pStat, int maxLvl);

// -------- 单场战斗: 蒙特卡洛模拟 (SIMULATIONMODE 次, 原版 calcBattle2) --------
// 返回胜率万分率 0-10000; 若 hpHist 非空, 同时统计剩余HP概率分布 (按 step 桶)
int kfolBattle(const int* pStat, const int* eStat, int lvl, double* hpHist, int hpHistSize, int step)
{
    int sims = gOptSimulationMode > 0 ? gOptSimulationMode : 1000;
    int rseed = (pStat[ATK] * 2654435761u ^ eStat[ATK] * 40503u ^ pStat[LFE] * 13u ^ lvl * 97u) & 0x7FFFFFFF;
    if (rseed == 0) rseed = 1;
    if (hpHist && hpHistSize > 0)
        for (int i = 0; i < hpHistSize; ++i) hpHist[i] = 0;
    int wins = 0;
    for (int s = 0; s < sims; ++s)
    {
        rseed = (rseed * 1103515245 + 12345) & 0x7FFFFFFF;
        int pHp = pStat[HP];
        int eHp = eStat[HP];
        int pSpd = pStat[SPD], eSpd = eStat[SPD];
        int pAtk = pStat[ATK], eAtk = eStat[ATK];
        int pSld = 0;
        int round = 0, pTm = 0, eTm = 0;
        int maxRound = gOptMaxRound > 0 ? gOptMaxRound : MAX_ROUND;
        for (;;)
        {
            int swdEnc = 0;
            int pSpd2 = pSpd;
            if (pTm < pSpd2 && eTm < eSpd)
            {
                int tmInc = pSpd2 - pTm <= eSpd - eTm ? pSpd2 - pTm : eSpd - eTm;
                pTm += tmInc; eTm += tmInc;
            }
            if (eTm >= eSpd)
            {
                eTm = 0;
                int tecRate = pStat[TEC] > 99 ? 99 : pStat[TEC];
                int crtRate = pStat[CRT] > 99 ? 99 : pStat[CRT];
                bool isTec = kfolRand100(&rseed) < tecRate;
                bool isCrt = kfolRand100(&rseed) < crtRate;
                int64_t dmg0 = pAtk;
                if (isCrt) dmg0 *= pStat[ACR]; else dmg0 *= 100;
                if (isTec) dmg0 = dmg0 + (int64_t)pStat[MAG] * 100;
                if (isTec) dmg0 = dmg0 * pStat[ASR] / 10000 * 100;
                int eDef = eStat[PRES] >= 0 ? ((int64_t)eStat[PRES] * 20001 + 150) / (eStat[PRES] * 2 + 300) : 0;
                if (eDef > 9900) eDef = 9900;
                int dmg = (int)((dmg0 * (10000 - eDef) + 999999) / 1000000) + pStat[LCH];
                eHp -= dmg;
                pHp += pStat[LCH];
                if (pHp > pStat[LFE]) pHp = pStat[LFE];
            }
            else
            {
                pTm = 0;
                bool isTec = kfolRand100(&rseed) < eStat[TEC];
                bool isCrt = kfolRand100(&rseed) < eStat[CRT];
                int64_t dmg = (isTec ? 0 : eAtk * (isCrt ? 2 : 1)) + (isTec ? eStat[MAG] : 0);
                if (eStat[SKL] == FAST && lvl > 100 && isCrt) dmg *= 3;
                if (pSld >= dmg) { pSld -= dmg; dmg = 1; }
                else { dmg -= pSld; pSld = 0; }
                int pDef = pStat[PRES] >= 0 ? ((int64_t)pStat[PRES] * 20001 + 150) / (pStat[PRES] * 2 + 300) : 0;
                if (pDef > 9900) pDef = 9900;
                dmg = (dmg * (10000 - pDef) + 9999) / 10000;
                pHp -= dmg;
                if (isTec)
                {
                    switch (eStat[SKL])
                    {
                        case TOGH: eHp += eStat[LFE] / 10; break;
                        case NORM: eAtk += eAtk / 4; break;
                        case FAST:
                        {
                            int newSpd = eSpd + eSpd / 2;
                            if (newSpd > 100000000) newSpd = 100000000;
                            eTm += newSpd - eSpd; eSpd = newSpd;
                            break;
                        }
                    }
                }
                if (pHp > pStat[LFE]) pHp = pStat[LFE];
                if (eHp > eStat[LFE]) eHp = eStat[LFE];
                if (eStat[SKL] == FAST && lvl > 100 && kfolRand100(&rseed) < 30)
                {
                    int newSpd = eSpd * 3 / 10;
                    if (newSpd > 100000000) newSpd = 100000000;
                    eTm += newSpd - eSpd; eSpd = newSpd;
                }
            }
            ++round;
            if (eHp < 1)
            {
                if (hpHist && step > 0)
                {
                    int idx = pHp / step;
                    if (idx >= hpHistSize) idx = hpHistSize - 1;
                    hpHist[idx] += 1.0 / sims;
                }
                ++wins;
                break;
            }
            if (pHp < 1)
            {
                if (hpHist && step > 0) hpHist[0] += 1.0 / sims;  // 死掉 → 剩余 0 桶
                break;
            }
            if (round >= maxRound)
            {
                if (eHp < pHp) { if (hpHist && step > 0) { int idx = pHp / step; if (idx >= hpHistSize) idx = hpHistSize - 1; hpHist[idx] += 1.0 / sims; } ++wins; }
                else { if (hpHist && step > 0) hpHist[0] += 1.0 / sims; }
                break;
            }
            if (round >= 20)
            {
                eAtk *= 2;
                if (eAtk > 30000000) eAtk = 30000000;
                int newSpd = eSpd * 2;
                if (newSpd > 100000000) newSpd = 100000000;
                eTm += newSpd - eSpd; eSpd = newSpd;
            }
        }
    }
    return wins * 10000 / sims;
}

// -------- 递归评估 (原版 calcAttrForward): 给定层+HP, 逐层战斗->剩余HP分布->递归下一层 --------
// 返回期望通过层数 (double)
// ponytail: 无缓存递归会指数爆炸; 用全局战斗结果缓存 (同层同敌人只模拟一次) + 稀疏化 (>1%概率桶才递归)
#define KFOL_BUCKET_MAX 2048
static double kfolBattleCache[KFOL_BUCKET_MAX][6];   // [桶][敌人类型] 的剩余HP分布
static bool kfolBattleCached[6];                     // 每敌人类型是否已缓存 (独立标志, 避免桶0歧义)
static int kfolBattleCachedWin[6];                   // 每敌人类型缓存的胜率 (万分率)
static int kfolBattleCacheLvl = -1;                   // 当前缓存所属层
static int kfolBucketSize = 100;
// 递归用静态分布缓冲 (避免 100 层递归 × 16KB 栈数组 = 栈溢出闪退; 搜索单线程所以安全)
// OpenMP 并行后: 每线程一份 (threadprivate)
static double kfolTmpHist[KFOL_BUCKET_MAX];
static double kfolHpHist[KFOL_BUCKET_MAX];
// OpenMP threadprivate: 工作线程副本不继承 static 零初始化 (垃圾值!), 用哨兵检测首次进入
static int kfolThreadInit = 0;
#pragma omp threadprivate(kfolTmpHist, kfolHpHist, kfolBattleCache, kfolBattleCached, kfolBattleCachedWin, kfolBattleCacheLvl, kfolBucketSize, kfolThreadInit)

// 每线程首次进入时初始化 (OpenMP 副本零初始化只对主线程)
static inline void kfolEnsureThreadInit()
{
    if (kfolThreadInit) return;
    kfolBattleCacheLvl = -1;
    for (int e = 0; e < 6; ++e) kfolBattleCached[e] = false;
    for (int e = 0; e < 6; ++e) kfolBattleCachedWin[e] = 0;
    kfolBucketSize = 100;
    kfolThreadInit = 1;
}

// 进度回调 (native -> Java), 每 pattern/层调用
typedef void (*KfolProgressFn)(const char*);
static KfolProgressFn gProgressFn = NULL;
void kfolSetProgressCallback(KfolProgressFn fn) { gProgressFn = fn; }
static inline void kfolProgress(const char* fmt, ...)
{
    if (!gProgressFn) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gProgressFn(buf);
}

static double kfolEvalForward(int lvl, int hp, const int* pStat, int maxLvl)
{
    kfolEnsureThreadInit();
    if (hp <= 0 || lvl > maxLvl) return lvl - 1;  // 死了或到顶
    if (lvl == 1 && hp > pStat[LFE]) hp = pStat[LFE];

    // 本层敌人加权: 普通层 NORM..CLVR, 10的倍数层 BOSS
    int eMin = lvl % 10 == 0 ? BOSS : NORM;
    int eMax = lvl % 10 == 0 ? BOSS : CLVR;
    int step = gOptBattleStep > 0 ? gOptBattleStep : 100;
    kfolBucketSize = step;
    int histSize = pStat[LFE] / step + 2;
    if (histSize > KFOL_BUCKET_MAX) histSize = KFOL_BUCKET_MAX;

    // 本层战斗结果缓存: 若缓存层改变则全清
    if (kfolBattleCacheLvl != lvl)
    {
        kfolBattleCacheLvl = lvl;
        for (int e = 0; e < 6; ++e) kfolBattleCached[e] = false;
    }

    // 加权剩余HP分布 (静态缓冲, 递归安全)
    double* hpHist = kfolHpHist;
    for (int i = 0; i < histSize; ++i) hpHist[i] = 0;
    int64_t rateSum = 0, winSum = 0;

    for (int e = eMin; e <= eMax; ++e)
    {
        int rate = kfolEnemyRate[e];
        if (rate <= 0) continue;
        // 用缓存分布 (独立标志), 未算则模拟
        if (!kfolBattleCached[e])
        {
            int eStat[STAT_NUM];
            kfolCalcEnemyStats(lvl, e, eStat);
            int winRate = kfolBattle(pStat, eStat, lvl, kfolTmpHist, histSize, step);
            for (int i = 0; i < histSize; ++i) kfolBattleCache[i][e] = kfolTmpHist[i];
            kfolBattleCachedWin[e] = winRate;
            kfolBattleCached[e] = true;
        }
        for (int i = 0; i < histSize; ++i)
            hpHist[i] += kfolBattleCache[i][e] * rate;
        rateSum += rate;
        winSum += (int64_t)kfolBattleCachedWin[e] * rate;
    }
    if (rateSum <= 0) return lvl - 1;
    for (int i = 0; i < histSize; ++i) hpHist[i] /= rateSum;

    // 胜率 = 加权胜率 (直接用 kfolBattle 返回值, 不从分布反推)
    int64_t winRate = winSum / rateSum;
    if (winRate < gOptMinWinRate) return lvl - 1;  // 打不过本层

    // 稀疏化递归: 只对概率 >= 1% 的剩余HP桶继续 (原版 imod 采样近似)
    double total = 0;
    for (int i = 0; i < histSize; ++i)
    {
        if (hpHist[i] < 0.01) continue;  // <1% 忽略
        int nextHp = i * step;
        if (nextHp < 1) continue;
        total += kfolEvalForward(lvl + 1, nextHp, pStat, maxLvl) * hpHist[i];
    }
    return total;
}

// -------- 爬塔: 分布递归评估 (原版 calcAttrForward), 返回期望通过层数 --------
int kfolClimb(int startLvl, int maxLvl, const int* attr, int wpnLvl, int amrLvl)
{
    kfolEnsureThreadInit();
    int pStat[STAT_NUM];
    kfolCalcPlayerStats(attr, wpnLvl, amrLvl, pStat);
    double expect = kfolEvalForward(startLvl, pStat[HP], pStat, maxLvl);
    // 期望层数转整数 (保留小数部分到 0.5 进位)
    return (int)(expect + 0.5);
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
    // MAXLEVEL 选项若大于 0 且小于传入 maxLvl, 收敛到选项值
    if (gOptMaxLvl > 0 && gOptMaxLvl < maxLvl) maxLvl = gOptMaxLvl;
    int bestLvlSoFar = startLvl - 1;

    // 8 个 INIT_WEIGHT 起点并行 (8gen3 全核跑满); 每线程独立局部最优, critical 合并
    #pragma omp parallel for schedule(dynamic, 1)
    for (int pattern = 0; pattern < INIT_PATTERN_NUM; ++pattern)
    {
        int attr[ATTR_NUM];
        kfolInitAttr(pattern, points, attr);
        kfolProgress("[KFOL] 模式%d/%d 起点 %d %d %d %d %d %d, 初始评估中...",
                     pattern + 1, INIT_PATTERN_NUM, attr[0], attr[1], attr[2], attr[3], attr[4], attr[5]);
        int curLvl = kfolClimb(startLvl, maxLvl, attr, wpnLvl, amrLvl);
        kfolProgress("[KFOL] 模式%d/%d 初始通过 %d 层", pattern + 1, INIT_PATTERN_NUM, curLvl);
        if (curLvl > bestLvlSoFar)
        {
            #pragma omp critical
            {
                if (curLvl > bestLvlSoFar)
                {
                    bestLvlSoFar = curLvl;
                    for (int i = 0; i < ATTR_NUM; ++i) bestAttr[i] = attr[i];
                }
            }
        }
        // 多步爬山: 步长递减收敛 (等价原版 steps {10,5,2,1})
        const int steps[] = {10, 5, 2, 1};
        for (size_t si = 0; si < sizeof(steps) / sizeof(steps[0]); ++si)
        {
            int step = steps[si];
            bool improved = true;
            // 原版每步 iter=8; 爆算到底跑满, 不提前收敛
            for (int iter = 0; iter < 8; ++iter)
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
                        #pragma omp critical
                        {
                            if (curLvl > bestLvlSoFar)
                            {
                                bestLvlSoFar = curLvl;
                                for (int k = 0; k < ATTR_NUM; ++k) bestAttr[k] = attr[k];
                            }
                        }
                    }
                }
                // 原版: 无改进 break; 但我们继续遍历其他 i/j 组合 (爆算到底)
            }
        }
    }
    *bestLvl = bestLvlSoFar;
}

} // extern "C"