# -*- coding: utf-8 -*-
# 验证 kfol_core.cpp 的核心算法逻辑 (与 C++ 逐行对齐, 含道具效果)
ATTR_NUM = 6
STAT_NUM = 12
ENEMY_NUM = 6
MAX_ROUND = 200

g_items = [0] * 6  # 蕾米 十六夜 钥匙 CD 药 券


def enemy_base(lvl, attr):
    coef = [
        [5,5,2,2,2,1], [10,7,3,3,3,4], [13,10,5,6,6,8],
        [30,30,20,20,20,20], [55,55,45,45,45,45],
        [75,75,55,55,55,55], [99,99,99,99,99,99]
    ]
    band = 0 if lvl <= 50 else 1 if lvl <= 100 else 2 if lvl <= 200 else \
           3 if lvl <= 210 else 4 if lvl <= 220 else 5 if lvl <= 230 else 6
    return lvl * coef[band][attr]


def calc_player_stats(attr, wpn_lvl, amr_lvl, aura=501, items=None):
    if items is None:
        items = g_items
    out = [0] * STAT_NUM
    b = attr[:]
    b[0] += items[0] + items[4] * 5
    b[1] += items[0] + items[4] * 5
    b[2] += items[1] + items[4] * 5
    b[3] += items[1] + items[4] * 5
    b[4] += items[4] * 5
    b[5] += items[4] * 5
    out[0] = b[0] * (1000 + aura) // 1000
    out[1] = b[1] * (1000 + aura) // 1000
    out[2] = b[2] * (1000 + aura) // 1000
    out[3] = b[3] * (1000 + aura) // 1000
    out[4] = b[4] * (1000 + aura) // 1000
    out[5] = b[5] * (1000 + aura) // 1000
    out[0] += wpn_lvl * 2
    out[2] += wpn_lvl
    out[1] += amr_lvl * 3
    out[6] = amr_lvl * 4
    if items[0] >= 50:
        out[1] += 700
    if items[1] >= 50:
        out[2] += 100
    out[11] = out[1] * 10 + 100
    return out


def calc_enemy_stats(lvl, etype, items=None):
    if items is None:
        items = g_items
    base = [enemy_base(lvl, i) for i in range(6)]
    boost = [
        [100,100,100,100,100,100], [300,300,100,100,100,100],
        [100,300,100,100,100,100], [100,100,500,300,300,100],
        [100,100,700,100,600,100], [300,200,300,600,600,600]
    ]
    base = [base[i] * boost[etype][i] // 100 for i in range(6)]
    out = [0] * STAT_NUM
    out[0] = base[0] * 5
    out[1] = base[1] * 5
    out[2] = base[2] * 3
    out[3] = base[3] * 2
    out[4] = base[4] * 2
    out[5] = base[5]
    out[11] = out[1] * 10
    # CD 减益
    if items[3] > 0:
        out[11] = out[11] * (1000 - items[3] * 8) // 1000
        if out[11] < 1:
            out[11] = 1
    if items[3] >= 30:
        out[0] = out[0] * 9 // 10
    return out


def battle(pstat, estat):
    php, ehp = pstat[11], estat[11]
    rounds = 0
    while ehp > 0 and rounds < MAX_ROUND:
        rounds += 1
        dmg = pstat[0] * (10000 - estat[6]) // 10000 + pstat[10]
        if dmg < 1:
            dmg = 1
        ehp -= dmg
        if ehp <= 0:
            break
        edmg = estat[0] * (10000 - pstat[6]) // 10000
        if edmg < 1:
            edmg = 1
        php -= edmg
        if php <= 0:
            return php
    return php if ehp <= 0 else 0


def climb(start, maxlvl, attr, wl, al, aura=501, items=None):
    if items is None:
        items = g_items
    ps = calc_player_stats(attr, wl, al, aura, items)
    lvl = start
    while lvl <= maxlvl:
        worst = -1
        for e in range(ENEMY_NUM):
            es = calc_enemy_stats(lvl, e, items)
            remain = battle(ps, es)
            if remain > worst:
                worst = remain
        if worst <= 0:
            break
        ps[11] = worst
        lvl += 1
    return lvl - 1


def greedy_search(points, start, maxlvl, wl, al, aura=501, items=None):
    if items is None:
        items = g_items
    attr = [points // 6] * 6
    attr[0] += points - points // 6 * 6
    cur = climb(start, maxlvl, attr, wl, al, aura, items)
    for _ in range(40):
        improved = False
        for i in range(6):
            for j in range(6):
                if i == j or attr[i] <= 1:
                    continue
                test = attr[:]
                test[i] -= 5
                test[j] += 5
                if test[i] < 1:
                    continue
                l = climb(start, maxlvl, test, wl, al, aura, items)
                if l > cur:
                    cur = l
                    attr = test
                    improved = True
        if not improved:
            break
    return attr, cur


# ---- 测试 ----
# 1. 敌人属性
assert enemy_base(50, 0) == 250 and enemy_base(50, 1) == 250
assert enemy_base(51, 0) == 510 and enemy_base(51, 1) == 357
# 2. 玩家属性基础
attr = [100] * 6
ps = calc_player_stats(attr, 12, 6)
assert ps[0] == 100 * 1501 // 1000 + 24
assert ps[11] == ps[1] * 10 + 100
# 3. 战斗边界
strong = [1000] * 6
weak = [1] * 6
es = calc_enemy_stats(1, 5)
assert battle(calc_player_stats(strong, 12, 6), es) > 0
assert battle(calc_player_stats(weak, 12, 6), es) <= 0
# 4. 爬塔
assert climb(1, 10, strong, 12, 6) >= 10
assert climb(1, 10, weak, 12, 6) <= 1
# 5. 搜索点数守恒
ba, bl = greedy_search(500, 1, 239, 12, 6)
assert bl >= 1 and sum(ba) == 500, f"sum={sum(ba)} lvl={bl}"
print(f"基础搜索: 通过层数={bl}")

# 6. 道具效果: 50漫画 -> 每本+1VIT(吃光环: +50*1501/1000=75) + 满50平加700LFE => diff=775
attr50 = [100] * 6
ps0 = calc_player_stats(attr50, 12, 6)
ps50 = calc_player_stats(attr50, 12, 6, items=[50, 0, 0, 0, 0, 0])
assert ps50[1] == ps0[1] + 775, f"LFE diff={ps50[1]-ps0[1]}"
assert ps50[11] == ps0[11] + 7750, f"HP diff={ps50[11]-ps0[11]}"
print(f"漫画50: LFE={ps0[1]}->{ps50[1]}, HP={ps0[11]}->{ps50[11]}")

# 7. 道具效果: 药 10瓶 全属性+50
ps_yao = calc_player_stats(attr50, 12, 6, items=[0, 0, 0, 0, 10, 0])
assert ps_yao[0] > ps0[0], f"药应加力量: {ps0[0]}->{ps_yao[0]}"
print(f"药10: ATK={ps0[0]}->{ps_yao[0]}")

# 8. 道具效果: CD 降敌生命/攻击
es0 = calc_enemy_stats(1, 0, items=[0, 0, 0, 0, 0, 0])
es_cd = calc_enemy_stats(1, 0, items=[0, 0, 0, 5, 0, 0])
es_cd30 = calc_enemy_stats(1, 0, items=[0, 0, 0, 30, 0, 0])
assert es_cd[11] < es0[11], "CD 应降敌生命"
assert es_cd30[0] < es0[0], "CD30 应降敌攻击"
print(f"CD5: 敌HP={es0[11]}->{es_cd[11]}; CD30: 敌ATK={es0[0]}->{es_cd30[0]}")

# 9. 道具齐全: 搜索仍然守恒
ba2, bl2 = greedy_search(500, 1, 239, 12, 6, items=[50, 50, 30, 30, 10, 10])
assert sum(ba2) == 500
print(f"满道具搜索: 通过层数={bl2}")

print("ALL CORE LOGIC TESTS PASSED")
