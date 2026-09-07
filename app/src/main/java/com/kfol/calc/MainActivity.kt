package com.kfol.calc

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.*

class MainActivity : AppCompatActivity() {

    private lateinit var npcRateStrgInput: EditText
    private lateinit var npcRateToghInput: EditText
    private lateinit var npcRateFastInput: EditText
    private lateinit var npcRateClvrInput: EditText
    private lateinit var coefInput: EditText
    private lateinit var auraInput: EditText
    private lateinit var hpHealInput: EditText
    private lateinit var hpStepInput: EditText
    private lateinit var pointsInput: EditText
    private lateinit var attrStrInput: TextView
    private lateinit var attrVitInput: TextView
    private lateinit var attrAgiInput: TextView
    private lateinit var attrDexInput: TextView
    private lateinit var attrIntInput: TextView
    private lateinit var attrResInput: TextView
    private lateinit var attrStaInput: EditText
    private lateinit var attrLukInput: EditText
    private lateinit var keyLevelsInput: EditText
    private lateinit var itemRemInput: EditText
    private lateinit var itemIzaInput: EditText
    private lateinit var itemKeyInput: EditText
    private lateinit var itemLolInput: EditText
    private lateinit var itemYaoInput: EditText
    private lateinit var itemZheInput: EditText
    private lateinit var modeFromFirst: RadioButton
    private lateinit var modeFromLvl: RadioButton
    private lateinit var startLvlInput: EditText
    private lateinit var maxLvlInput: EditText
    private lateinit var wpnLvlInput: EditText
    private lateinit var amrLvlInput: EditText
    private lateinit var neckLvlInput: EditText
    private lateinit var equipInput: EditText
    private lateinit var equipInfoView: TextView
    private lateinit var equipHelpBtn: Button
    private lateinit var pasteBtn: Button
    private lateinit var parseEquipBtn: Button
    private lateinit var optInput: EditText
    private lateinit var optHelpBtn: Button
    private lateinit var pasteOptBtn: Button
    private lateinit var resultView: TextView
    private lateinit var statusView: TextView
    private lateinit var searchBtn: Button

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var calcJob: Job? = null
    private var attrFillJob: Job? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        npcRateStrgInput = findViewById(R.id.npcRateStrgInput)
        npcRateToghInput = findViewById(R.id.npcRateToghInput)
        npcRateFastInput = findViewById(R.id.npcRateFastInput)
        npcRateClvrInput = findViewById(R.id.npcRateClvrInput)
        coefInput = findViewById(R.id.coefInput)
        auraInput = findViewById(R.id.auraInput)
        hpHealInput = findViewById(R.id.hpHealInput)
        hpStepInput = findViewById(R.id.hpStepInput)
        pointsInput = findViewById(R.id.pointsInput)
        attrStrInput = findViewById(R.id.attrStrInput)
        attrVitInput = findViewById(R.id.attrVitInput)
        attrAgiInput = findViewById(R.id.attrAgiInput)
        attrDexInput = findViewById(R.id.attrDexInput)
        attrIntInput = findViewById(R.id.attrIntInput)
        attrResInput = findViewById(R.id.attrResInput)
        attrStaInput = findViewById(R.id.attrStaInput)
        attrLukInput = findViewById(R.id.attrLukInput)
        keyLevelsInput = findViewById(R.id.keyLevelsInput)
        itemRemInput = findViewById(R.id.itemRemInput)
        itemIzaInput = findViewById(R.id.itemIzaInput)
        itemKeyInput = findViewById(R.id.itemKeyInput)
        itemLolInput = findViewById(R.id.itemLolInput)
        itemYaoInput = findViewById(R.id.itemYaoInput)
        itemZheInput = findViewById(R.id.itemZheInput)
        modeFromFirst = findViewById(R.id.modeFromFirst)
        modeFromLvl = findViewById(R.id.modeFromLvl)
        startLvlInput = findViewById(R.id.startLvlInput)
        maxLvlInput = findViewById(R.id.maxLvlInput)
        wpnLvlInput = findViewById(R.id.wpnLvlInput)
        amrLvlInput = findViewById(R.id.amrLvlInput)
        neckLvlInput = findViewById(R.id.neckLvlInput)
        equipInput = findViewById(R.id.equipInput)
        equipInfoView = findViewById(R.id.equipInfoView)
        equipHelpBtn = findViewById(R.id.equipHelpBtn)
        pasteBtn = findViewById(R.id.pasteBtn)
        parseEquipBtn = findViewById(R.id.parseEquipBtn)
        optInput = findViewById(R.id.optInput)
        optHelpBtn = findViewById(R.id.optHelpBtn)
        pasteOptBtn = findViewById(R.id.pasteOptBtn)
        resultView = findViewById(R.id.resultView)
        statusView = findViewById(R.id.statusView)
        searchBtn = findViewById(R.id.searchBtn)

        // 默认值 (与 kfol.in 推荐一致), 已有存档则用存档
        val prefs = getSharedPreferences("kfol_inputs", MODE_PRIVATE)
        fun restore(id: String, view: android.widget.TextView, def: String) {
            view.setText(prefs.getString(id, def))
        }
        restore("npcStrg", npcRateStrgInput, "10")
        restore("npcTogh", npcRateToghInput, "10")
        restore("npcFast", npcRateFastInput, "10")
        restore("npcClvr", npcRateClvrInput, "10")
        restore("coef", coefInput, "11")
        restore("aura", auraInput, "501")
        restore("hpHeal", hpHealInput, "8")
        restore("hpStep", hpStepInput, "100")
        restore("points", pointsInput, "500")
        // 最佳加点 8 维 (只读输出, 存档主要是保持上次结果)
        restore("aStr", attrStrInput, "1")
        restore("aVit", attrVitInput, "1")
        restore("aAgi", attrAgiInput, "1")
        restore("aDex", attrDexInput, "1")
        restore("aInt", attrIntInput, "1")
        restore("aRes", attrResInput, "1")
        restore("aSta", attrStaInput, "0")
        restore("aLuk", attrLukInput, "0")
        restore("keyLevels", keyLevelsInput, "1,51,101,151,201")
        restore("maxLvl", maxLvlInput, "239")
        restore("startLvl", startLvlInput, "1")
        restore("wpnLvl", wpnLvlInput, "12")
        restore("amrLvl", amrLvlInput, "6")
        restore("neckLvl", neckLvlInput, "0")
        restore("iRem", itemRemInput, "0")
        restore("iIza", itemIzaInput, "0")
        restore("iKey", itemKeyInput, "0")
        restore("iLol", itemLolInput, "0")
        restore("iYao", itemYaoInput, "0")
        restore("iZhe", itemZheInput, "0")
        restore("optText", optInput, "")

        // 药/券上限 10: 输入完成后检查完整文本, 超上限回写 10
        fun clampMax10(view: EditText) {
            view.addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    val v = s?.toString()?.toIntOrNull() ?: return
                    if (v > 10) view.setText("10")
                }
            })
        }
        clampMax10(itemYaoInput)
        clampMax10(itemZheInput)

        // 高级选项默认值: 用户提供的配置
        optInput.setText(
            "MAXROUND 15\nFASTSKILL 4\nTOUGHSKILL 0\n" +
            "MAXLEVEL 100\nGRIDOPTION 6 4 6\nBATTLESTEP 1\n" +
            "MINWINRATE 100\nSERVERBONUS 1\nSIMULATIONMODE 1000\nVERBOSE 1"
        )

        // 模式切换
        modeFromFirst.setOnCheckedChangeListener { _, checked ->
            startLvlInput.isEnabled = !checked
        }

        // 初始化基础参数 (NPC出现率/神秘系数/光环/HP参数)
        applyBaseParams()

        // 分配点数输入 -> 不自动填 (最佳加点只在点"开始计算"后输出)

        // 一键粘贴装备
        pasteBtn.setOnClickListener {
            pasteFromClipboard(equipInput) { "已粘贴装备 $it 字符" }
        }

        // 解析装备
        parseEquipBtn.setOnClickListener {
            val raw = equipInput.text.toString().trim()
            if (raw.isEmpty()) { toast("先粘贴装备文本"); return@setOnClickListener }
            val info = EquipmentParser.parse(raw)
            if (info == null) {
                equipInfoView.text = "解析失败: 无法识别装备类型 (Fist/Sword/Bow/Staff/Plate/Leather/Cloth)"
                return@setOnClickListener
            }
            equipInfoView.text = EquipmentParser.summarize(info)
            if (info.isWeapon && wpnLvlInput.text.toString().isBlank()) wpnLvlInput.setText("12")
            if (!info.isWeapon && amrLvlInput.text.toString().isBlank()) amrLvlInput.setText("6")
            toast("解析成功: ${info.type} #${info.id}")
        }

        // 一键粘贴高级选项
        pasteOptBtn.setOnClickListener {
            pasteFromClipboard(optInput) { "已粘贴高级选项 $it 字符" }
        }

        // 装备粘贴帮助: 告诉用户去哪里复制
        equipHelpBtn.setOnClickListener {
            android.app.AlertDialog.Builder(this)
                .setTitle("装备粘贴说明")
                .setMessage(
                    "1. 打开 绯月 → 我的物品\n" +
                    "2. 选中要计算的装备 (武器/防具)\n" +
                    "3. 点击「复制装备参数」\n" +
                    "4. 回到本App, 粘贴到上方输入框\n" +
                    "5. 点「解析装备并加入计算」\n\n" +
                    "支持武器: Fist/Sword/Bow/Staff\n" +
                    "支持防具: Plate(铠甲)/Leather(皮甲)/Cloth(布甲)\n" +
                    "可同时贴多件装备, 每行一件。"
                )
                .setPositiveButton("知道了", null)
                .show()
        }

        // 高级选项帮助: 逐项说明
        optHelpBtn.setOnClickListener {
            android.app.AlertDialog.Builder(this)
                .setTitle("高级选项说明")
                .setMessage(
                    "MAXROUND N — 单场战斗最大回合数\n" +
                    "FASTSKILL N — 快速怪技能 (0~4)\n" +
                    "TOUGHSKILL N — 坚韧怪技能\n" +
                    "MAXLEVEL N — 搜索最高层数\n" +
                    "GRIDOPTION a b c — 搜索网格参数 (半径 步长 中心)\n" +
                    "BATTLESTEP N — HP 离散步长 (越小越精细)\n" +
                    "MINWINRATE N — 最低胜率要求 (0~100)\n" +
                    "SERVERBONUS N — 服务器攻击加成 (0/1/2)\n" +
                    "SIMULATIONMODE N — 蒙特卡洛样本数 (0=精确)\n" +
                    "VERBOSE 0/1 — 详细日志\n\n" +
                    "不填则使用默认值, 格式与绯月 kfol.in 一致。"
                )
                .setPositiveButton("知道了", null)
                .show()
        }

        searchBtn.setOnClickListener { startSearch() }
    }

    /** 防抖: 点数输入停止 400ms 后触发 native 搜索, 自动填 8 维 */
    private fun applyBaseParams() {
        // 4 个出现率: 强壮/坚强/快速/睿智 (剩余为普通)
        val rates = intArrayOf(
            npcRateStrgInput.text.toString().toIntOrNull() ?: 10,
            npcRateToghInput.text.toString().toIntOrNull() ?: 10,
            npcRateFastInput.text.toString().toIntOrNull() ?: 10,
            npcRateClvrInput.text.toString().toIntOrNull() ?: 10
        )
        val coef = coefInput.text.toString().toIntOrNull() ?: 11
        val aura = auraInput.text.toString().toIntOrNull() ?: 501
        val hpHeal = hpHealInput.text.toString().toIntOrNull() ?: 8
        val hpStep = hpStepInput.text.toString().toIntOrNull() ?: 100
        NativeCore.setFullParams(rates, aura, coef, hpHeal, hpStep)
        // 高级选项 (kfol.in 格式, C++ 侧解析生效)
        NativeCore.setOptions(optInput.text.toString())
    }

    /** 填入 6 维搜索结果 (原版搜索变量仅 STR/VIT/AGI/DEX/INT/RES; STA/LUK 用户自定, 计算器不碰) */
    private fun fillAttrFields(attr6: IntArray, points: Int) {
        attrStrInput.setText(attr6[0].toString())
        attrVitInput.setText(attr6[1].toString())
        attrAgiInput.setText(attr6[2].toString())
        attrDexInput.setText(attr6[3].toString())
        attrIntInput.setText(attr6[4].toString())
        attrResInput.setText(attr6[5].toString())
    }

    private fun readItems(): IntArray {
        // 药/券上限 10 (满10即最高效果)
        val yao = (itemYaoInput.text.toString().toIntOrNull() ?: 0).coerceAtMost(10)
        val zhe = (itemZheInput.text.toString().toIntOrNull() ?: 0).coerceAtMost(10)
        return intArrayOf(
            itemRemInput.text.toString().toIntOrNull() ?: 0,
            itemIzaInput.text.toString().toIntOrNull() ?: 0,
            itemKeyInput.text.toString().toIntOrNull() ?: 0,
            itemLolInput.text.toString().toIntOrNull() ?: 0,
            yao,
            zhe
        )
    }

    private fun pasteFromClipboard(target: EditText, msg: (Int) -> String) {
        val cm = getSystemService(CLIPBOARD_SERVICE) as android.content.ClipboardManager
        val clip = cm.primaryClip
        if (clip != null && clip.itemCount > 0) {
            val txt = clip.getItemAt(0).coerceToText(this).toString()
            target.setText(txt)
            toast(msg(txt.length))
        } else {
            toast("剪贴板为空")
        }
    }

    private fun startSearch() {
        val points = pointsInput.text.toString().toIntOrNull() ?: 500
        val startLvl = startLvlInput.text.toString().toIntOrNull() ?: 1
        val maxLvl = maxLvlInput.text.toString().toIntOrNull() ?: 239
        val wpnLvl = wpnLvlInput.text.toString().toIntOrNull() ?: 12
        val amrLvl = amrLvlInput.text.toString().toIntOrNull() ?: 6
        val aura = auraInput.text.toString().toIntOrNull() ?: 501
        val items = readItems()

        // 道具加成由 C++ 内部处理 (钥匙/药加点是原版 attrPoints 的一部分)
        val effPoints = points

        if (points < 6 || maxLvl < 1) { toast("参数不合法"); return }

        calcJob?.cancel()
        searchBtn.isEnabled = false
        statusView.text = "计算中 (CPU+GPU 并行)..."
        resultView.text = ""

        calcJob = scope.launch {
            val res = withContext(Dispatchers.Default) {
                NativeCore.runSearch(effPoints,
                    if (modeFromFirst.isChecked) 1 else startLvl,
                    maxLvl, wpnLvl, amrLvl, aura, items)
            }
            val names = arrayOf("力量", "体质", "敏捷", "灵活", "智力", "意志", "耐力", "幸运")
            // 计算结果自动回填到加点输入框 (仅 6 维搜索变量, STA/LUK 用户自定不覆盖)
            fillAttrFields(res.attr, effPoints)
            val sb = StringBuilder()
            val attr8 = IntArray(8)
            System.arraycopy(res.attr, 0, attr8, 0, 6)
            // STA/LUK 用用户输入值 (原版: 非搜索变量, 用户指定)
            attr8[6] = attrStaInput.text.toString().toIntOrNull() ?: 0
            attr8[7] = attrLukInput.text.toString().toIntOrNull() ?: 0
            attr8.forEachIndexed { i, v -> sb.append("${names[i]}: $v\n") }
            sb.append("最优通过层数: ${res.bestLvl}\n")
                        sb.append("加点总点数: $points\n")
            sb.append("道具: 漫画${items[0]}/${items[1]} 钥匙${items[2]} CD${items[3]} 药${items[4]} 券${items[5]}\n")
            sb.append("NPC出现率: 强壮${npcRateStrgInput.text} 坚强${npcRateToghInput.text} 快速${npcRateFastInput.text} 睿智${npcRateClvrInput.text}\n")
            sb.append("神秘系数: ${coefInput.text} 光环: $aura\n")
            sb.append("HP回复: ${hpHealInput.text} HPstep: ${hpStepInput.text}\n")
            sb.append("耗时: ${res.elapsedMs} ms\n")
            sb.append("计算引擎: ${if (res.gpuUsed) "GPU(OpenCL)+CPU" else "CPU(OpenMP)"}")
            resultView.text = sb.toString()
            statusView.text = "完成"
            searchBtn.isEnabled = true
        }
    }

    private fun toast(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }

    override fun onDestroy() {
        // 保存所有输入 (覆盖安装/重启后不丢)
        try {
            val e = getSharedPreferences("kfol_inputs", MODE_PRIVATE).edit()
            e.putString("npcStrg", npcRateStrgInput.text.toString())
            e.putString("npcTogh", npcRateToghInput.text.toString())
            e.putString("npcFast", npcRateFastInput.text.toString())
            e.putString("npcClvr", npcRateClvrInput.text.toString())
            e.putString("coef", coefInput.text.toString())
            e.putString("aura", auraInput.text.toString())
            e.putString("hpHeal", hpHealInput.text.toString())
            e.putString("hpStep", hpStepInput.text.toString())
            e.putString("points", pointsInput.text.toString())
            e.putString("aStr", attrStrInput.text.toString())
            e.putString("aVit", attrVitInput.text.toString())
            e.putString("aAgi", attrAgiInput.text.toString())
            e.putString("aDex", attrDexInput.text.toString())
            e.putString("aInt", attrIntInput.text.toString())
            e.putString("aRes", attrResInput.text.toString())
            e.putString("aSta", attrStaInput.text.toString())
            e.putString("aLuk", attrLukInput.text.toString())
            e.putString("keyLevels", keyLevelsInput.text.toString())
            e.putString("maxLvl", maxLvlInput.text.toString())
            e.putString("startLvl", startLvlInput.text.toString())
            e.putString("wpnLvl", wpnLvlInput.text.toString())
            e.putString("amrLvl", amrLvlInput.text.toString())
            e.putString("neckLvl", neckLvlInput.text.toString())
            e.putString("iRem", itemRemInput.text.toString())
            e.putString("iIza", itemIzaInput.text.toString())
            e.putString("iKey", itemKeyInput.text.toString())
            e.putString("iLol", itemLolInput.text.toString())
            e.putString("iYao", itemYaoInput.text.toString())
            e.putString("iZhe", itemZheInput.text.toString())
            e.putString("optText", optInput.text.toString())
            e.apply()
        } catch (_: Exception) {}
        super.onDestroy()
        scope.cancel()
    }
}