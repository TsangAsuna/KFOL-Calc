# -*- coding: utf-8 -*-
# 验证 kfol_core.cpp 新的原版搜索算法 (INIT_WEIGHT + 多步爬山)
# 复用 test_core.py 已验证的战斗公式, 只测搜索层
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_core import calc_player_stats, calc_enemy_stats, battle, climb, g_items

ATTR_NUM = 6

INIT_WEIGHT = [
    [0, 0, 1, 0, 0, 0], [1, 1, 1, 1, 1, 1], [1, 1, 1, 0, 0, 0],
    [1, 0, 4, 2, 0, 0], [1, 0, 1, 0, 4, 0], [0, 0, 2, 0, 4, 2],
    [1, 1, 3, 3, 3, 0], [3, 1, 3, 0, 0, 3],
]


def init_attr(pattern, points):
    a = [0] * 6
    wsum = sum(INIT_WEIGHT[pattern])
    base = points - 6
    for i in range(6):
        a[i] = base * INIT_WEIGHT[pattern][i] // wsum + 1
        if a[i] < 1:
            a[i] = 1
        points -= a[i]
    if points > 0:
        inc_w = True
        while points:
            changed = False
            for i in range(6):
                if points and (not inc_w or INIT_WEIGHT[pattern][i] > 0):
                    a[i] += 1
                    points -= 1
                    changed = True
            if not changed:
                inc_w = False
    return a


def search_attrs(points, start, maxlvl, wpn, amr, aura, items):
    best_lvl = start - 1
    best_attr = None
    for pattern in range(8):
        attr = init_attr(pattern, points)
        cur = climb(start, maxlvl, attr, wpn, amr, aura, items)
        if cur > best_lvl:
            best_lvl = cur
            best_attr = list(attr)
        for step in (10, 5, 2, 1):
            improved = True
            for _ in range(8):
                if not improved:
                    break
                improved = False
                bi = bj = -1
                best_delta = cur
                for i in range(6):
                    if attr[i] - step < 1:
                        continue
                    attr[i] -= step
                    for j in range(6):
                        if j == i:
                            continue
                        attr[j] += step
                        l = climb(start, maxlvl, attr, wpn, amr, aura, items)
                        if l > best_delta:
                            best_delta = l
                            bi, bj = i, j
                        attr[j] -= step
                    attr[i] += step
                if bi != -1 and best_delta > cur:
                    attr[bi] -= step
                    attr[bj] += step
                    cur = best_delta
                    improved = True
                    if cur > best_lvl:
                        best_lvl = cur
                        best_attr = list(attr)
    return best_lvl, best_attr


print("=== 测试 1: 8 模式 Init 总点守恒 ===")
for pat in range(8):
    a = init_attr(pat, 500)
    assert sum(a) == 500, f"pattern{pat} sum={sum(a)}"
    assert all(x >= 1 for x in a), f"pattern{pat} min<1: {a}"
print("8/8 守恒 OK")
print("  敏捷型(pattern0) =", init_attr(0, 500))
print("  智力型(pattern4) =", init_attr(4, 500))
assert init_attr(0, 500)[2] > init_attr(4, 500)[2], "敏捷型起点 AGI 应高于智力型"
assert init_attr(4, 500)[4] > init_attr(0, 500)[4], "智力型起点 INT 应高于敏捷型"

print("\n=== 测试 2: 搜索能爬塔 ===")
lvl1, attr1 = search_attrs(500, 1, 239, 12, 6, 501, [0] * 6)
print(f"500点 wpn12 amr6: 最优层 {lvl1}, 加点 {attr1}")
assert lvl1 >= 1 and attr1 is not None and sum(attr1) == 500, f"失败: lvl={lvl1} attr={attr1}"

print("\n=== 测试 3: 装备越强 爬得越高 ===")
lvl_weak, _ = search_attrs(500, 1, 239, 6, 3, 501, [0] * 6)
lvl_strong, _ = search_attrs(500, 1, 239, 40, 40, 501, [0] * 6)
print(f"弱装备(lv6/3): 层 {lvl_weak}, 强装备(lv40/40): 层 {lvl_strong}")
assert lvl_strong >= lvl_weak, f"装备越强应越高: {lvl_strong} < {lvl_weak}"

print("\n=== 测试 4: 点数越多 爬得越高 ===")
lvl_low, _ = search_attrs(300, 1, 239, 12, 6, 501, [0] * 6)
lvl_high, _ = search_attrs(2000, 1, 239, 12, 6, 501, [0] * 6)
print(f"300点: {lvl_low}, 2000点: {lvl_high}")
assert lvl_high >= lvl_low, f"点数多应更高: {lvl_high} < {lvl_low}"

print("\n=== 测试 5: 新搜索(多爬山) >= 旧搜索(贪心5) 的层数 ===")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib, test_core
lvl_new, attr_new = search_attrs(500, 1, 239, 12, 6, 501, [0] * 6)
attr_old, lvl_old = test_core.greedy_search(500, 1, 239, 12, 6, 501, [0] * 6)
print(f"新搜索(INIT_WEIGHT+多步): {lvl_new} [加点 {attr_new}]")
print(f"旧搜索(平均+贪心5):      {lvl_old} [加点 {attr_old}]")
assert lvl_new >= lvl_old, f"原版算法应不劣于旧贪心: {lvl_new} < {lvl_old}"

print("\n=== 测试 6: 道具提升 ===")
lvl_no, _ = search_attrs(500, 1, 239, 12, 6, 501, [0] * 6)
lvl_items, attr_items = search_attrs(500, 1, 239, 12, 6, 501, [50, 50, 30, 30, 10, 10])
print(f"无道具: {lvl_no}, 满道具: {lvl_items}, 加点 {attr_items}")
assert lvl_items >= lvl_no, f"满道具应不低: {lvl_items} < {lvl_no}"

print("\nALL SEARCH TESTS PASSED")