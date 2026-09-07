# -*- coding: utf-8 -*-
# 对齐新 kfol_core.cpp (原版算法移植) 的核心逻辑测试
# 覆盖: 玩家属性链 / 敌人属性 / 四分支期望战斗 / 爬塔 / INIT_WEIGHT+多步爬山搜索
import sys, os

ATTR_NUM = 6
STAT_NUM = 12
ENEMY_NUM = 6
MAX_LVL = 240
MAX_ROUND = 200

g_items = [0] * 6
g_aura = 501
g_coef = 11
# 敌人出现率 (原版: 强壮 坚强 快速 睿智 各10, 普通=剩余60, BOSS=100)
ENEMY_RATES = [60, 10, 10, 10, 10, 100]
MIN_WIN_RATE = 100  # 万分率 0-10000 (用户默认 MINWINRATE 100 = 1%胜率要求, 原版同语义)

ENEMY_NUM_NAMES = ["NORM", "STRG", "TOGH", "FAST", "CLVR", "BOSS"]

def enemy_base(lvl, attr):
    coef = [
        [5,5,2,2,2,1], [10,7,3,3,3,4], [13,10,5,6,6,8],
        [30,30,20,20,20,20], [55,55,45,45,45,45],
        [75,75,55,55,55,55], [99,99,99,99,99,99]
    ]
    band = 0 if lvl <= 50 else 1 if lvl <= 100 else 2 if lvl <= 200 else \
           3 if lvl <= 210 else 4 if lvl <= 220 else 5 if lvl <= 230 else 6
    return lvl * coef[band][attr]

ENEMY_BOOST = [
    [(1,1),(1,1),(1,1),(1,1),(1,1),(1,1)],
    [(3,1),(3,2),(1,1),(3,10),(3,10),(3,10)],
    [(1,1),(3,1),(3,10),(3,10),(3,10),(3,2)],
    [(3,10),(3,10),(5,1),(3,1),(3,10),(3,10)],
    [(3,10),(3,10),(7,10),(3,10),(6,1),(3,10)],
    [(3,2),(2,1),(3,2),(6,5),(6,5),(6,5)]
]

def calc_player(attr, wpn, amr):
    a = list(attr)
    a[0] += g_items[0] + g_items[4] * 5
    a[1] += g_items[0] + g_items[4] * 5
    a[2] += g_items[1] + g_items[4] * 5
    a[3] += g_items[1] + g_items[4] * 5
    a[4] += g_items[4] * 5
    a[5] += g_items[4] * 5
    pn = [x + x * g_aura // 1000 for x in a]
    wc = [5,2,1,1,1,3]
    ac = [5,20,1,1,10,10]
    ab = [100,500,0,150,0,0]
    wpnval = [wc[i] * wpn * (1000 + 0) // 1000 for i in range(6)]
    amrval = [ac[i] * amr * (1000 + 0) // 1000 + ab[i] for i in range(6)]
    out = [0] * 12
    out[0] = pn[0] * 5 + wpnval[0]
    out[1] = pn[1] * 20
    out[2] = pn[2] * 2 + wpnval[1]
    out[3] = (pn[3] * 201 + 100) // (pn[3] * 2 + 200) + wpnval[2]
    out[3] = min(out[3], 99)
    out[4] = (pn[4] * 201 + 90) // (pn[4] * 2 + 180) + wpnval[3]
    out[4] = min(out[4], 99)
    out[5] = (pn[1] + pn[4]) * 4 + wpnval[0]
    out[6] = pn[5]
    out[8] = 200 + wpnval[2]
    out[9] = 100 + wpnval[3]
    out[10] = wpnval[4] * 50000 // 50000
    out[11] = out[1] + 100
    if g_items[0] >= 50:
        out[1] += 700
        out[11] = out[1] + 100
    if g_items[1] >= 50:
        out[2] += 100
    return out

def calc_enemy(lvl, etype):
    base = [enemy_base(lvl, i) * ENEMY_BOOST[etype][i][0] // ENEMY_BOOST[etype][i][1] for i in range(6)]
    out = [0] * 12
    out[0] = base[0] * 3
    out[1] = base[1] * 20
    out[2] = base[2] * 2
    out[3] = (base[3] * 201 + 100) // (base[3] * 2 + 200)
    out[3] = min(out[3], 99)
    out[4] = (base[4] * 201 + 90) // (base[4] * 2 + 180)
    out[4] = min(out[4], 99)
    out[5] = base[0] * 3 if etype == 1 else base[4] * 15 if etype == 4 else 0
    out[6] = base[5]
    out[8] = 200
    out[9] = 100
    out[11] = out[1]
    if g_items[3] > 0:
        out[11] = (out[11] * (250 - g_items[3] * 2) + 125) // 250
        if out[11] < 1:
            out[11] = 1
    if g_items[3] >= 30:
        out[0] = (out[0] * 9 + 5) // 10
    return out

def def_v(pres):
    if pres <= 0:
        return 0
    d = (pres * 20001 + 150) // (pres * 2 + 300)
    return min(d, 9900)

def rand100(state):
    M = 48271
    Q = 0x7FFFFFFF // M
    R = 0x7FFFFFFF % M
    state[0] = M * (state[0] % Q) - R * (state[0] // Q)
    if state[0] < 0:
        state[0] += 0x7FFFFFFF
    return state[0] % 100

def battle_one(state, ps, es, lvl):
    php, ehp = ps[11], es[11]
    pspd, espd = ps[2], es[2]
    patk, eatk = ps[0], es[0]
    psld = 0
    maxround = MAX_ROUND
    round, ptm, etm = 0, 0, 0
    while True:
        pspd2 = pspd
        if ptm < pspd2 and etm < espd:
            inc = min(pspd2 - ptm, espd - etm)
            ptm += inc; etm += inc
        if etm >= espd:  # 玩家回合
            etm = 0
            tec = min(ps[4], 99); crt = min(ps[3], 99)
            istec = rand100(state) < tec
            iscrt = rand100(state) < crt
            dmg0 = patk * ps[8] if iscrt else patk * 100
            if istec:
                dmg0 = dmg0 + ps[5] * 100
                dmg0 = dmg0 * ps[9] // 10000 * 100
            edef = def_v(es[6])
            dmg = (dmg0 * (10000 - edef) + 999999) // 1000000 + ps[10]
            ehp -= dmg
            php += ps[10]
            if php > ps[1]:
                php = ps[1]
        else:  # 敌人回合
            ptm = 0
            istec = rand100(state) < es[4]
            iscrt = rand100(state) < es[3]
            dmg = (0 if istec else eatk * (2 if iscrt else 1)) + (es[5] if istec else 0)
            if es[3] == 0 and iscrt:
                dmg = 0
            if es[10] == 3 and lvl > 100 and iscrt:
                dmg *= 3
            if psld >= dmg:
                psld -= dmg; dmg = 1
            else:
                dmg -= psld; psld = 0
            pdef = def_v(ps[6])
            dmg = (dmg * (10000 - pdef) + 9999) // 10000
            php -= dmg
            if istec:
                if es[10] == 2:  # TOGH
                    ehp += es[1] // 10
                elif es[10] == 0:  # NORM
                    eatk += eatk // 4
                elif es[10] == 3:  # FAST
                    newspd = espd + espd // 2
                    espd = min(newspd, 100000000)
                    etm += newspd - espd
            if php > ps[1]:
                php = ps[1]
            if ehp > es[1]:
                ehp = es[1]
            if es[10] == 3 and lvl > 100 and rand100(state) < 30:
                newspd = espd * 3 // 10
                espd = min(newspd, 100000000)
                etm += newspd - espd
        round += 1
        if ehp < 1:
            return 1
        if php < 1:
            return 0
        if round >= maxround:
            return 1 if ehp < php else 0
        if round >= 20:
            eatk = min(eatk * 2, 30000000)
            newspd = espd * 2
            espd = min(newspd, 100000000)
            etm += newspd - espd

def battle(ps, es, lvl, sims=100):
    state = [(ps[0] * 2654435761 ^ es[0] * 40503 ^ ps[1] * 13 ^ lvl * 97) & 0x7FFFFFFF]
    if state[0] == 0:
        state[0] = 1
    wins = 0
    for _ in range(sims):
        state[0] = (state[0] * 1103515245 + 12345) & 0x7FFFFFFF
        wins += battle_one(state, ps, es, lvl)
    return wins * 10000 // sims

def climb(start, maxlvl, attr, wpn, amr, sims=100):
    ps = calc_player(attr, wpn, amr)
    lvl = start
    while lvl <= maxlvl:
        emin = 5 if lvl % 10 == 0 else 0
        emax = 5 if lvl % 10 == 0 else 4
        wsum = wwin = 0
        for e in range(emin, emax + 1):
            rate = ENEMY_RATES[e]
            if rate <= 0:
                continue
            es = calc_enemy(lvl, e)
            winrate = battle(ps, es, lvl, sims)
            wsum += rate
            wwin += winrate * rate
        if wsum <= 0:
            break
        avg = wwin // wsum
        if avg < MIN_WIN_RATE:
            break
        hp = ps[1] * avg // 10000 + 100
        ps[11] = min(hp, ps[1])
        lvl += 1
    return lvl - 1

INIT_WEIGHT = [
    [0,0,1,0,0,0], [1,1,1,1,1,1], [1,1,1,0,0,0],
    [1,0,4,2,0,0], [1,0,1,0,4,0], [0,0,2,0,4,2],
    [1,1,3,3,3,0], [3,1,3,0,0,3]
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

def search(points, start, maxlvl, wpn, amr, aura, items, sims=100):
    global g_aura, g_items
    g_aura = aura
    g_items = list(items)
    best_lvl = start - 1
    best_attr = None
    for pattern in range(8):
        attr = init_attr(pattern, points)
        cur = climb(start, maxlvl, attr, wpn, amr, sims)
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
                        l = climb(start, maxlvl, attr, wpn, amr, sims)
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

if __name__ == '__main__':
    ok = True
    def t(cond, msg):
        global ok
        print(('PASS ' if cond else 'FAIL ') + msg)
        if not cond:
            ok = False

    print('--- 单元: 玩家属性链 ---')
    attr = [100]*6
    ps = calc_player(attr, 12, 6)
    t(ps[0] > 500, f'ATK={ps[0]} > 500 (STR100*5=500+装备)')
    t(ps[1] >= 2000, f'LFE={ps[1]} >= VIT100*20=2000')
    t(ps[11] > ps[1], f'HP={ps[11]} > LFE')
    t(0 <= ps[3] <= 99, f'CRT={ps[3]} 在0-99')
    t(0 <= ps[4] <= 99, f'TEC={ps[4]} 在0-99')

    print('--- 单元: 敌人属性 ---')
    for e in range(ENEMY_NUM):
        es = calc_enemy(50, e)
        t(es[0] > 0 and es[11] > 0, f'层50 {ENEMY_NUM_NAMES[e]}: ATK={es[0]} LFE={es[1]} HP={es[11]}')

    print('--- 单元: 战斗 (胜率万分率) ---')
    winner = battle(calc_player([1000]*6, 100, 100), calc_enemy(10, 0), 10)
    t(winner > 5000, f'超人打层10 NORM: 胜率 {winner}/10000')
    loser = battle(calc_player([1,1,1,1,1,1], 1, 1), calc_enemy(200, 5), 200)
    t(loser <= 5000, f'菜鸟打层200 BOSS: 胜率 {loser}/10000 (低)')

    print('--- 单元: 爬塔 (小样本10, 限100层) ---')
    lvl_weak = climb(1, 100, [10]*6, 1, 1, sims=10)
    lvl_strong = climb(1, 100, [1000]*6, 100, 100, sims=10)
    t(lvl_strong >= lvl_weak, f'强属性爬更高: weak={lvl_weak} strong={lvl_strong}')

    print('--- 搜索 (小样本10, 限100层) ---')
    lvl500, attr500 = search(500, 1, 100, 12, 6, 501, [0]*6, sims=10)
    t(attr500 is not None and sum(attr500) == 500, f'搜索500点: 层{lvl500} 加点守恒={sum(attr500) if attr500 else None}')
    lvl2000, _ = search(2000, 1, 100, 12, 6, 501, [0]*6, sims=10)
    t(lvl2000 >= lvl500, f'2000点({lvl2000}) >= 500点({lvl500})')
    lvl_strong_equip, _ = search(500, 1, 100, 40, 40, 501, [0]*6, sims=10)
    t(lvl_strong_equip >= lvl500, f'强装备({lvl_strong_equip}) >= 默认装备({lvl500})')

    print('\n' + ('ALL KFOL CORE TESTS PASSED' if ok else 'SOME TESTS FAILED'))
    sys.exit(0 if ok else 1)