package com.kfol.calc

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.*

class MainActivity : AppCompatActivity() {

    private lateinit var npcRatesInput: EditText
    private lateinit var coefInput: EditText
    private lateinit var auraInput: EditText
    private lateinit var hpHealInput: EditText
    private lateinit var hpStepInput: EditText
    private lateinit var pointsInput: EditText
    private lateinit var attrStrInput: EditText
    private lateinit var attrVitInput: EditText
    private lateinit var attrAgiInput: EditText
    private lateinit var attrDexInput: EditText
    private lateinit var attrIntInput: EditText
    private lateinit var attrResInput: EditText
    private lateinit var attrStaInput: EditText
    private lateinit var attrLukInput: EditText
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
    private lateinit var pasteBtn: Button
    private lateinit var parseEquipBtn: Button
    private lateinit var optInput: EditText
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

        npcRatesInput = findViewById(R.id.npcRatesInput)
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
        pasteBtn = findViewById(R.id.pasteBtn)
        parseEquipBtn = findViewById(R.id.parseEquipBtn)
        optInput = findViewById(R.id.optInput)
        pasteOptBtn = findViewById(R.id.pasteOptBtn)
        resultView = findViewById(R.id.resultView)
        statusView = findViewById(R.id.statusView)
        searchBtn = findViewById(R.id.searchBtn)

        // 默认值 (与 kfol.in 推荐一致)
        npcRatesInput.setText("10 10 10 10")
        coefInput.setText("11")
        auraInput.setText("501")
        hpHealInput.setText("8")
        hpStepInput.setText("100")
        pointsInput.setText("500")
        maxLvlInput.setText("239")
        startLvlInput.setText("1")
        wpnLvlInput.setText("12")
        amrLvlInput.setText("6")
        neckLvlInput.setText("0")
        itemRemInput.setText("0")
        itemIzaInput.setText("0")
        itemKeyInput.setText("0")
        itemLolInput.setText("0")
        itemYaoInput.setText("0")
        itemZheInput.setText("0")

        // 高级选项默认值: readme 推荐配置
        optInput.setText(
            "LIMIT 1 200\nVIT MAX 150\nENDLIMIT\n\n" +
            "MAXROUND 15\nFASTSKILL 4\nTOUGHSKILL 1\n" +
            "BATTLESTEP 10\nGRIDOPTION 6 3 5\nMINWINRATE 100"
        )

        // 模式切换
        modeFromFirst.setOnCheckedChangeListener { _, checked ->
            startLvlInput.isEnabled = !checked
        }

        // 初始化基础参数 (NPC出现率/神秘系数/光环/HP参数)
        applyBaseParams()

        // 分配点数输入 -> 实时计算最佳加点并自动填入
        pointsInput.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(s: Editable?) {
                val points = s?.toString()?.toIntOrNull() ?: return
                if (points < 6) return
                scheduleAttrFill(points)
            }
        })

        // 一键粘贴装备
        pasteBtn.setOnClickListener {
            pasteFromClipboard(equipInput) { "已粘贴装备 ${it.length} 字符" }
        }

        // 解析装备
        parseEquipBtn.setOnClickListener {
            val raw = equipInput.text.toString().trim()
            if (raw.isEmpty()) { toast("先粘贴装备文本"); return@setOnClickListener }
            val info = EquipmentParser.parse(raw)
            if (info == null) {
                equipInfoView.text = "解析失败: 无法识别装备类型 (Fist/Sword/Bow/Staff/Body/Plate/Leather/Cloth)"
                return@setOnClickListener
            }
            equipInfoView.text = EquipmentParser.summarize(info)
            if (info.isWeapon && wpnLvlInput.text.toString().isBlank()) wpnLvlInput.setText("12")
            if (!info.isWeapon && amrLvlInput.text.toString().isBlank()) amrLvlInput.setText("6")
            toast("解析成功: ${info.type} #${info.id}")
        }

        // 一键粘贴高级选项
        pasteOptBtn.setOnClickListener {
            pasteFromClipboard(optInput) { "已粘贴高级选项 ${it.length} 字符" }
        }

        searchBtn.setOnClickListener { startSearch() }
    }

    /** 防抖: 点数输入停止 400ms 后触发 native 搜索, 自动填 8 维 */
    private fun applyBaseParams() {
        val rates = npcRatesInput.text.toString().split(Regex("\\s+"))
            .mapNotNull { it.toIntOrNull() }.toIntArray()
        val coef = coefInput.text.toString().toIntOrNull() ?: 11
        val aura = auraInput.text.toString().toIntOrNull() ?: 501
        val hpHeal = hpHealInput.text.toString().toIntOrNull() ?: 8
        val hpStep = hpStepInput.text.toString().toIntOrNull() ?: 100
        NativeCore.setFullParams(rates, aura, coef, hpHeal, hpStep)
    }

    private fun scheduleAttrFill(points: Int) {
        attrFillJob?.cancel()
        attrFillJob = scope.launch {
            delay(400)
            val wpn = wpnLvlInput.text.toString().toIntOrNull() ?: 12
            val amr = amrLvlInput.text.toString().toIntOrNull() ?: 6
            val aura = auraInput.text.toString().toIntOrNull() ?: 501
            val items = readItems()
            val res = withContext(Dispatchers.Default) {
                NativeCore.runSearch(points, 1, maxLvlInput.text.toString().toIntOrNull() ?: 239,
                    wpn, amr, aura, items)
            }
            fillAttrFields(res.attr, points)
            statusView.text = "已自动计算加点 (${res.elapsedMs}ms, ${if (res.gpuUsed) "GPU+CPU" else "CPU"})"
        }
    }

    /** 填入 8 维 (native 返回 6 主属性, 耐力/幸运按剩余点数启发分配) */
    private fun fillAttrFields(attr6: IntArray, points: Int) {
        val used = attr6.sum()
        val rest = points - used
        attrStrInput.setText(attr6[0].toString())
        attrVitInput.setText(attr6[1].toString())
        attrAgiInput.setText(attr6[2].toString())
        attrDexInput.setText(attr6[3].toString())
        attrIntInput.setText(attr6[4].toString())
        attrResInput.setText(attr6[5].toString())
        attrStaInput.setText((rest / 2).toString())
        attrLukInput.setText((rest - rest / 2).toString())
    }

    private fun readItems(): IntArray = intArrayOf(
        itemRemInput.text.toString().toIntOrNull() ?: 0,
        itemIzaInput.text.toString().toIntOrNull() ?: 0,
        itemKeyInput.text.toString().toIntOrNull() ?: 0,
        itemLolInput.text.toString().toIntOrNull() ?: 0,
        itemYaoInput.text.toString().toIntOrNull() ?: 0,
        itemZheInput.text.toString().toIntOrNull() ?: 0
    )

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

        // 道具点数加成
        val itemPointsBonus =
            (if (items[2] >= 30) 30 else 0) + (if (items[4] >= 10) 120 else 0)
        val effPoints = points + itemPointsBonus

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
            val sb = StringBuilder()
            val attr8 = IntArray(8)
            System.arraycopy(res.attr, 0, attr8, 0, 6)
            val used = res.attr.sum()
            val rest = effPoints - used
            attr8[6] = rest / 2
            attr8[7] = rest - rest / 2
            attr8.forEachIndexed { i, v -> sb.append("${names[i]}: $v\n") }
            sb.append("\n最优通过层数: ${res.bestLvl}\n")
            sb.append("加点总点数: $effPoints (含道具加成 $itemPointsBonus)\n")
            sb.append("道具: 漫画${items[0]}/${items[1]} 钥匙${items[2]} CD${items[3]} 药${items[4]} 券${items[5]}\n")
            sb.append("NPC出现率: ${npcRatesInput.text}\n")
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
        super.onDestroy()
        scope.cancel()
    }
}