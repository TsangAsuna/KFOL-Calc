// kfol_cpu.cpp - CPU 并行调度辅助 (OpenMP)
// 核心搜索逻辑在 kfol_core.cpp; 本文件负责多候选点的并行评估
#include <string.h>
#include <stdlib.h>
#include <vector>
using namespace std;

extern "C" {
int kfolClimb(int, int, const int*, int, int);

// 并行评估多个候选加点, 返回每个候选的通过层数
// candidates: [n][6], n 个候选
void kfolCpuEvalCandidates(const int* candidates, int n, int startLvl, int maxLvl,
                           int wpnLvl, int amrLvl, int* outLvls)
{
#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n; ++i)
        outLvls[i] = kfolClimb(startLvl, maxLvl, candidates + i * 6, wpnLvl, amrLvl);
}
} // extern "C"