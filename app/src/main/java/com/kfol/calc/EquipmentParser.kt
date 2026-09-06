package com.kfol.calc

/**
 * 绯月装备文本解析器
 * 格式 (与 kfol2 一致):
 *   Cloth #90442450            <- 类型(8种) + #ID
 *   1 IMT                      <- 神秘属性数 + 神秘属性名
 *   4 AMR RFL CRD SLD          <- 主属性数 + 主属性名
 *   8 HEL COF 5 HEL COF 2 ...  <- 从属性条目数 + (属性 类型 数值) 三元组
 */
object EquipmentParser {

    // 8 种装备类型: 前 4 武器, 后 4 防具
    val WEAPON_TYPES = listOf("Fist", "Sword", "Bow", "Staff")
    val ARMOR_TYPES = listOf("Body", "Plate", "Leather", "Cloth")

    // 中文映射
    val TYPE_CN = mapOf(
        "Fist" to "拳套", "Sword" to "长剑", "Bow" to "短弓", "Staff" to "法杖",
        "Body" to "布衣", "Plate" to "铁甲", "Leather" to "皮甲", "Cloth" to "布甲"
    )
    // 神秘属性: 武器 FMT/LMT/AMT, 防具 IMT/TMT/HMT
    val MYSTIC_CN = mapOf(
        "FMT" to "武器增幅", "LMT" to "武器减耗", "AMT" to "武器攻速",
        "IMT" to "防具减伤", "TMT" to "防具坚韧", "HMT" to "防具回复"
    )

    // 武器主属性 / 防具主属性 (kfol 定义)
    val WEAPON_MAIN = listOf("ATK", "SPD", "CRT", "SKL", "BRC", "LCH")
    val ARMOR_MAIN = listOf("HEL", "SLD", "AMR", "RFL", "CRD", "SRD")
    val MAIN_CN = mapOf(
        "ATK" to "攻击", "SPD" to "攻速", "CRT" to "暴击", "SKL" to "技能", "BRC" to "破防", "LCH" to "吸血",
        "HEL" to "生命", "SLD" to "护盾", "AMR" to "护甲", "RFL" to "反弹", "CRD" to "暴抗", "SRD" to "闪避"
    )
    // 从属性类型: 武器 COF/STR/AGI/INT, 防具 COF/VIT/DEX/RES
    val SUB_TYPES_WEAPON = listOf("COF", "STR", "AGI", "INT")
    val SUB_TYPES_ARMOR = listOf("COF", "VIT", "DEX", "RES")
    val SUB_CN = mapOf(
        "COF" to "系数", "STR" to "力量", "AGI" to "敏捷", "INT" to "智力",
        "VIT" to "体质", "DEX" to "灵活", "RES" to "意志"
    )

    data class EquipInfo(
        val type: String,       // 英文类型名
        val isWeapon: Boolean,
        val id: String,         // # 后面的 ID
        val mystic: List<String>,   // 神秘属性
        val main: List<String>,     // 主属性
        val sub: List<Triple<String, String, Int>>, // (主属性, 从属性类型, 数值)
        val raw: String
    )

    fun parse(text: String): EquipInfo? {
        val lines = text.lines().map { it.trim() }.filter { it.isNotEmpty() }
        if (lines.isEmpty()) return null

        // 行1: "Cloth #90442450"
        val first = lines[0]
        val type = WEAPON_TYPES.firstOrNull { first.startsWith(it) }
            ?: ARMOR_TYPES.firstOrNull { first.startsWith(it) }
            ?: return null
        val isWeapon = type in WEAPON_TYPES
        val id = first.substringAfter('#').trim().takeWhile { it.isDigit() || it.isLetter() }

        var idx = 1
        // 神秘属性: "1 IMT"
        var mystic = emptyList<String>()
        if (idx < lines.size) {
            val parts = lines[idx].split(Regex("\\s+")).filter { it.isNotEmpty() }
            if (parts.size >= 2 && parts[0].toIntOrNull() != null) {
                mystic = parts.drop(1)
                idx++
            }
        }
        // 主属性: "4 AMR RFL CRD SLD"
        var main = emptyList<String>()
        if (idx < lines.size) {
            val parts = lines[idx].split(Regex("\\s+")).filter { it.isNotEmpty() }
            if (parts.size >= 2 && parts[0].toIntOrNull() != null) {
                main = parts.drop(1)
                idx++
            }
        }
        // 从属性: "8 HEL COF 5 HEL COF 2 ..."
        var sub = emptyList<Triple<String, String, Int>>()
        if (idx < lines.size) {
            val parts = lines[idx].split(Regex("\\s+")).filter { it.isNotEmpty() }
            val n = parts.firstOrNull()?.toIntOrNull() ?: 0
            if (n > 0) {
                val rest = parts.drop(1)
                val tmp = mutableListOf<Triple<String, String, Int>>()
                var i = 0
                while (i + 2 < rest.size && tmp.size < n) {
                    val attr = rest[i]
                    val subType = rest[i + 1]
                    val v = rest[i + 2].toIntOrNull() ?: 0
                    tmp.add(Triple(attr, subType, v))
                    i += 3
                }
                sub = tmp
            }
        }

        return EquipInfo(type, isWeapon, id, mystic, main, sub, text)
    }

    /** 生成摘要 (UI 展示用, 含中文映射) */
    fun summarize(e: EquipInfo): String {
        val sb = StringBuilder()
        val kind = if (e.isWeapon) "武器" else "防具"
        sb.append("$kind: ${e.type}(${TYPE_CN[e.type]}) #${e.id}\n")
        sb.append("神秘属性: ${if (e.mystic.isEmpty()) "无" else e.mystic.joinToString(" ") { "$it(${MYSTIC_CN[it] ?: "?"})" }}\n")
        sb.append("主属性(${e.main.size}): ${e.main.joinToString(" ") { "$it(${MAIN_CN[it] ?: "?"})" }}\n")
        sb.append("从属性(${e.sub.size}):\n")
        e.sub.forEach { (a, t, v) -> sb.append("  $a(${MAIN_CN[a] ?: a}) $t(${SUB_CN[t] ?: t}) $v\n") }
        sb.append("\n神秘系数说明: COF 系数参与装备强化计算(enhance=COF×神秘系数×3+属性加成)\n")
        return sb.toString()
    }
}
