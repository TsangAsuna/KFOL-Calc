package com.kfol.calc

import android.os.SystemClock

/**
 * JNI 桥接: Kotlin <-> C++ 战斗核心
 * CPU + GPU 联合计算: GPU 跑蒙特卡洛样本, CPU 跑精确搜索
 */
object NativeCore {

    init {
        System.loadLibrary("kfolcalc")
    }

    external fun setParams(rates: IntArray, aura: Int, coef: Int)

    /**
     * 设置完整基础参数
     * @param npcRates NPC出现率[4] (强壮 坚强 快速 睿智)
     * @param aura 战力光环(千分率)
     * @param coef 神秘系数
     * @param hpHeal HP回复量
     * @param hpStep HP step
     */
    external fun setFullParams(npcRates: IntArray, aura: Int, coef: Int,
                               hpHeal: Int, hpStep: Int)

    /**
     * 设置高级选项 (kfol.in 格式原文, C++ 侧解析)
     * MAXROUND/FASTSKILL/TOUGHSKILL/MAXLEVEL/GRIDOPTION/BATTLESTEP/
     * MINWINRATE/SERVERBONUS/SIMULATIONMODE/VERBOSE
     */
    external fun setOptions(options: String)

    /** 注册进度监听 (native 计算中回调 Kotlin onProgress) */
    external fun setProgressListener(listener: Any?)

    /** native 进度回调 (由 JNI 从工作线程调用, 需 post 回主线程) */
    @JvmStatic
    fun onProgress(msg: String) {
        listener?.invoke(msg)
    }

    /** 由 MainActivity 设置, 回调到 UI */
    @Volatile
    var listener: ((String) -> Unit)? = null

    /** 探测 GPU 可用性: 1=可用 0=不可用 */
    external fun gpuAvailable(): Int

    /**
     * 自动加点搜索
     * @param points 可分配点数(已含道具加成)
     * @param startLvl 起始层
     * @param maxLvl 最大层
     * @param wpnLvl 武器等级
     * @param amrLvl 护甲等级
     * @param aura 战力光环(千分率)
     * @param items 道具数量[6]: 蕾米漫画 十六夜漫画 钥匙 CD 药 券
     * @return [attr0..5, bestLvl]
     */
    external fun searchAttrs(
        points: Int, startLvl: Int, maxLvl: Int, wpnLvl: Int, amrLvl: Int,
        aura: Int, items: IntArray
    ): IntArray

    /** 单层战斗评估: 返回 6 种敌人的剩余HP (<=0 失败) */
    external fun evaluateLayer(
        attr: IntArray, hp: Int, lvl: Int, wpnLvl: Int, amrLvl: Int
    ): IntArray

    /** GPU/CPU 联合蒙特卡洛胜率: 返回万分率 0-10000 */
    external fun mcWinRate(
        attr: IntArray, eAttr: IntArray, eStat: IntArray, eRate: IntArray,
        enemyNum: Int, samplesPerEnemy: Int
    ): Int

    /**
     * 带计时/调度的一键搜索
     * @return (bestAttr, bestLvl, elapsedMs, gpuUsed)
     */
    fun runSearch(
        points: Int, startLvl: Int, maxLvl: Int, wpnLvl: Int, amrLvl: Int,
        aura: Int, items: IntArray
    ): SearchResult {
        val t0 = SystemClock.elapsedRealtime()
        val gpu = gpuAvailable()
        val res = searchAttrs(points, startLvl, maxLvl, wpnLvl, amrLvl, aura, items)
        val elapsed = SystemClock.elapsedRealtime() - t0
        return SearchResult(
            attr = res.copyOfRange(0, 6),
            bestLvl = res[6],
            elapsedMs = elapsed,
            gpuUsed = gpu == 1
        )
    }

    data class SearchResult(
        val attr: IntArray,
        val bestLvl: Int,
        val elapsedMs: Long,
        val gpuUsed: Boolean
    )
}