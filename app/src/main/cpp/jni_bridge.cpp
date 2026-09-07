// jni_bridge.cpp - JNI 桥接层
// Kotlin (UI/调度) <-> C++ (战斗核心) <-> OpenCL (GPU)
#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
using namespace std;

#define LOG_TAG "kfolcalc"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {
extern int kfolEnemyRate[6];
extern int kfolAura;
extern int kfolCoef;
int kfolGpuInit(void);
int kfolGpuAvailable(void);
void kfolCpuMcSimulate(const int*, const int*, const int*, const int*, const int*,
                       int, int, float*, int*);
extern "C" void kfolSetOptions(const char* text);
void kfolSearchAttrs(int, int, int, int, int, int, const int*, int*, int*);
void kfolSetItems(const int*);
void kfolSetHpParams(int, int);
void kfolApplyItemDebuff(int*);
void kfolCalcEnemyStats(int, int, int*);
int kfolBattle(const int*, const int*);
int kfolClimb(int, int, const int*, int, int);
}

// 工具: jintArray -> vector
static void getIntArray(JNIEnv* env, jintArray arr, vector<int>& out)
{
    jsize n = env->GetArrayLength(arr);
    out.resize(n);
    env->GetIntArrayRegion(arr, 0, n, out.data());
}

// 设置全局参数: enemyRates[6], aura, coef
extern "C" JNIEXPORT void JNICALL
Java_com_kfol_calc_NativeCore_setParams(JNIEnv* env, jobject, jintArray rates, jint aura, jint coef)
{
    vector<int> r;
    getIntArray(env, rates, r);
    for (int i = 0; i < 6 && i < (int)r.size(); ++i) kfolEnemyRate[i] = r[i];
    kfolAura = aura;
    kfolCoef = coef;
}

// 设置完整基础参数: npcRates[4] = 强壮 坚强 快速 睿智, aura, coef, hpHeal, hpStep
extern "C" JNIEXPORT void JNICALL
Java_com_kfol_calc_NativeCore_setFullParams(JNIEnv* env, jobject,
    jintArray npcRates, jint aura, jint coef, jint hpHeal, jint hpStep)
{
    vector<int> r;
    getIntArray(env, npcRates, r);
    // UI 四框: 强壮(STRG) 坚强(TOGH) 快速(FAST) 睿智(CLVR)
    kfolEnemyRate[1] = r.size() > 0 ? r[0] : 10;   // STRG 强壮
    kfolEnemyRate[2] = r.size() > 1 ? r[1] : 10;   // TOGH 坚强
    kfolEnemyRate[3] = r.size() > 2 ? r[2] : 10;   // FAST 快速
    kfolEnemyRate[4] = r.size() > 3 ? r[3] : 10;   // CLVR 睿智
    // NORM 普通 = 100 - 四类之和 (下限 0)
    int norm = 100 - (kfolEnemyRate[1] + kfolEnemyRate[2] + kfolEnemyRate[3] + kfolEnemyRate[4]);
    kfolEnemyRate[0] = norm > 0 ? norm : 0;
    kfolEnemyRate[5] = 10;  // BOSS (每10层固定)
    kfolAura = aura;
    kfolCoef = coef;
    extern void kfolSetHpParams(int, int);
    kfolSetHpParams(hpHeal, hpStep);
}

// 设置高级选项: kfol.in 格式原文, C++ 侧解析
extern "C" JNIEXPORT void JNICALL
Java_com_kfol_calc_NativeCore_setOptions(JNIEnv* env, jobject, jstring opts)
{
    if (!opts) { extern "C" void kfolSetOptions(const char*); kfolSetOptions(NULL); return; }
    const char* str = env->GetStringUTFChars(opts, NULL);
    if (str)
    {
        extern "C" void kfolSetOptions(const char*);
        kfolSetOptions(str);
        env->ReleaseStringUTFChars(opts, str);
    }
}

// 探测 GPU: 返回 1 可用 / 0 不可用(回退CPU)
extern "C" JNIEXPORT jint JNICALL
Java_com_kfol_calc_NativeCore_gpuAvailable(JNIEnv*, jobject)
{
    return kfolGpuInit();
}

// 自动加点搜索: points, startLvl, maxLvl, wpnLvl, amrLvl, aura, items[6]
// 返回 int[7] = [bestAttr0..5, bestLvl]
extern "C" JNIEXPORT jintArray JNICALL
Java_com_kfol_calc_NativeCore_searchAttrs(JNIEnv* env, jobject,
    jint points, jint startLvl, jint maxLvl, jint wpnLvl, jint amrLvl,
    jint aura, jintArray itemsArr)
{
    vector<int> items;
    getIntArray(env, itemsArr, items);
    int items6[6] = {0};
    for (int i = 0; i < 6 && i < (int)items.size(); ++i) items6[i] = items[i];
    kfolAura = aura;

    int attr[6] = {0};
    int bestLvl = 0;
    extern void kfolSearchAttrs(int, int, int, int, int, int, const int*, int*, int*);
    kfolSearchAttrs(points, startLvl, maxLvl, wpnLvl, amrLvl, aura, items6, attr, &bestLvl);
    jintArray res = env->NewIntArray(7);
    jint tmp[7];
    for (int i = 0; i < 6; ++i) tmp[i] = attr[i];
    tmp[6] = bestLvl;
    env->SetIntArrayRegion(res, 0, 7, tmp);
    return res;
}

// 单层战斗评估: attr[6], hp, lvl, wpnLvl, amrLvl
// 返回 int[ENEMY_NUM] 各敌人剩余HP (<=0 失败)
extern "C" JNIEXPORT jintArray JNICALL
Java_com_kfol_calc_NativeCore_evaluateLayer(JNIEnv* env, jobject,
    jintArray attrArr, jint hp, jint lvl, jint wpnLvl, jint amrLvl)
{
    vector<int> attr;
    getIntArray(env, attrArr, attr);
    int pStat[12];
    kfolCalcPlayerStats(attr.data(), wpnLvl, amrLvl, pStat);
    pStat[11] = hp;
    jintArray res = env->NewIntArray(6);
    jint tmp[6];
    extern int kfolBattle(const int*, const int*);
    for (int e = 0; e < 6; ++e)
    {
        int eStat[12];
        extern void kfolCalcEnemyStats(int, int, int*);
        kfolCalcEnemyStats(lvl, e, eStat);
        tmp[e] = kfolBattle(pStat, eStat);
    }
    env->SetIntArrayRegion(res, 0, 6, tmp);
    return res;
}

// GPU/CPU 联合蒙特卡洛: 返回总胜率估算 (0-10000 万分率)
// GPU 可用则 GPU 算, 否则 CPU OpenMP 并行
extern "C" JNIEXPORT jint JNICALL
Java_com_kfol_calc_NativeCore_mcWinRate(JNIEnv* env, jobject,
    jintArray attrArr, jintArray eAttrArr, jintArray eStatArr,
    jintArray eRateArr, jint enemyNum, jint samplesPerEnemy)
{
    vector<int> attr, eAttr, eStat, eRate;
    getIntArray(env, attrArr, attr);
    getIntArray(env, eAttrArr, eAttr);
    getIntArray(env, eStatArr, eStat);
    getIntArray(env, eRateArr, eRate);
    int pStat[12];
    kfolCalcPlayerStats(attr.data(), 0, 0, pStat);

    vector<float> winRate(enemyNum);
    vector<int> winCount(enemyNum);
    int gpuOk = kfolGpuAvailable();
    if (gpuOk)
    {
        extern int kfolGpuSimulate(const int*, const int*, const int*, const int*, const int*, int, float*, int*);
        if (kfolGpuSimulate(attr.data(), pStat, eAttr.data(), eStat.data(),
                            eRate.data(), enemyNum, winRate.data(), winCount.data()) == 0)
        {
            double total = 0, wSum = 0;
            for (int e = 0; e < enemyNum; ++e)
            {
                total += winRate[e] * eRate[e];
                wSum += eRate[e];
            }
            return (jint)(total / wSum * 10000 + 0.5);
        }
    }
    // CPU 回退 (OpenMP)
    kfolCpuMcSimulate(attr.data(), pStat, eAttr.data(), eStat.data(), eRate.data(),
                      enemyNum, samplesPerEnemy, winRate.data(), winCount.data());
    double total = 0, wSum = 0;
    for (int e = 0; e < enemyNum; ++e)
    {
        total += winRate[e] * eRate[e];
        wSum += eRate[e];
    }
    LOGI("mcWinRate: gpu=%d win=%d", gpuOk, (int)(total / wSum * 10000));
    return (jint)(total / wSum * 10000 + 0.5);
}