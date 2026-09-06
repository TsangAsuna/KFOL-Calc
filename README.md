# KFOL 爬塔加点计算器 (Android)

简易安卓 App：计算魔塔(KFOL)爬塔的最优加点方案。

## CPU + GPU 联合计算设计

| 引擎 | 职责 |
|---|---|
| **GPU (OpenCL)** | 蒙特卡洛战斗模拟 —— 每个 work-item 模拟一整场战斗，海量独立样本天然并行 |
| **CPU (OpenMP)** | 精确战斗模拟 + 全层加点搜索 + 任务调度 —— 有依赖链的搜索逻辑 |

- GPU 可用 → 先用 GPU 快速估算胜率筛选候选，再用 CPU 精确爬山
- GPU 不可用（设备无 OpenCL 驱动）→ 自动回退纯 CPU OpenMP 并行

## 构建

本机无 Android 工具链，用 GitHub Actions 自动构建：

1. 推送到 GitHub 仓库（main 分支），Actions 自动构建
2. 或本地构建：
```bash
./gradlew assembleDebug
```

## 项目结构

```
app/src/main/
├── java/com/kfol/calc/
│   ├── MainActivity.kt    # UI + 参数输入 + 结果展示
│   └── NativeCore.kt      # JNI 接口 + CPU/GPU 调度
├── cpp/
│   ├── kfol_core.cpp      # 战斗模拟核心 (属性公式/NPC/爬塔)
│   ├── kfol_cpu.cpp       # CPU 并行评估 (OpenMP)
│   ├── kfol_gpu.cpp       # GPU 蒙特卡洛内核 (OpenCL)
│   └── jni_bridge.cpp     # JNI 桥接
└── res/                   # 布局/资源
```

## 计算逻辑 (与 readme 对齐)

- NPC 属性: 层数 x 系数，分 1-50/51-100/101-200/200+ 档
- 类型强化: 强壮/快速/虚弱/缓慢/BOSS
- 战斗: 攻击 = ATK x (10000 - 敌DEF) / 1000000，HP 扣减，回合制
- 加点搜索: 贪心爬山，每轮微调 5% 点数选收益最大的方向

## 参数说明

| 参数 | 含义 | 默认 |
|---|---|---|
| 总点数 | 可分配属性点 | 500 |
| 起始层数 | 从哪层开始 | 1 |
| 最大层数 | 超过视为打不赢 | 239 |
| 武器等级 / 护甲等级 | 装备等级加成 | 12 / 6 |

## 局限与升级路径

- 当前战斗模拟为确定性简化版（无暴击/技能概率采样细节）
- 完整版应移植 kfol_native_full.cpp 的 calcBattle1/2 精确概率状态机
- GPU 内核已是蒙特卡洛形态，换精确内核只需替换 kfol_gpu.cpp 的 simBattle
- OpenCL 驱动探测为尽力而为：高通/联发科 GPU 有 OpenCL，但个别设备需厂商驱动支持