# 2026-07-06 改动总结

> 本次改动覆盖 **7 个源文件**，包含 4 个主要功能模块：HRV 异常值过滤、呼吸引导动效、压力提示屏、阈值校准。

---

## 目录

1. [改动一：HRV 计算层 RR 异常值过滤](#一hrv-计算层-rr-异常值过滤)
2. [改动二：峰值检测改进尝试与撤回](#二峰值检测改进尝试与撤回已撤回)
3. [改动三：呼吸引导放松训练](#三呼吸引导放松训练)
4. [改动四：临时提示屏](#四临时提示屏)
5. [改动五：压力等级阈值校准](#五压力等级阈值校准)
6. [改动六：屏2 HRV 详情页独立刷新通道](#六屏2-hrv-详情页独立刷新通道)
7. [改动七：传感器 FIFO 溢出修复及性能优化](#七传感器-fifo-溢出修复及性能优化)
8. [完整文件改动清单](#完整文件改动清单)

---

## 一、HRV 计算层 RR 异常值过滤

### 为什么改

静息基线 HRV 指标严重异常（RMSSD=303, SDNN=237, pNN50=37.5%），远超健康成人正常范围（RMSSD 20–50, SDNN 30–60, pNN50 5–20）。

**根因**：MAX30102 手指定点 PPG 信号极其干净，收缩峰后约 200–400ms 出现的重搏切迹（dicrotic notch）幅值越过峰值检测的 30% 阈值，被误检为一次心跳。一个假拍产生两个异常 RR 间期（~200ms 短 + ~580ms 长），RMSSD 爆炸。

为什么手指放得越稳反而越严重：静息基线时手指最稳定，信号最干净，重搏切迹最清晰可见 → 假拍最多；实时测量时手指轻微移动，噪声淹没了切迹 → 假拍反而少。

### 怎么改

在峰值检测层改进是首选方向，但尝试 blanking 后 BPM 从 55–60 掉到 37（见下方第二节），证明峰值检测改动风险太高。改为在 HRV 计算层做防御性过滤——不影响心率计算。

### 改了什么

**文件**：`hrv_engine.cpp`

1. 新增 `#include <math.h>`，提供 `fabsf` 函数
2. 在 `hrv_engine_compute()` 中，窗口数据提取后、7 项指标计算前，插入中位数偏差过滤

**过滤算法**：

```
输入: tempNN[] = [780, 200, 580, 770, 790, 210, 570, 775, ...]
                   正常  假拍对↑     正常  正常  假拍对↑    正常

1. 在副本上排序求中位数 median
2. 遍历 tempNN[]:
   如果 |RR - median| / median > 30%
      → 跳过 RR[i] 和 RR[i+1] (成对剔除, 因为一个假拍污染两个连续RR)
   否则保留
3. 过滤后至少 20 个 (原 30 个) 才算有效
```

**参数选择**：静息 RR 波动 <6%，30% 阈值足够宽不会误杀正常值。成对剔除（`i+=2`）是必须的——假拍的 200ms 和 580ms 各自偏离中位数 74% 和 26%，如果只剔 200ms 保留 580ms，RMSSD 仍然偏高。

**最低 NN 数**：从 30 降到 20，因为已过质量筛选，20 个干净 RR 足以统计。

### 不影响的部分

- 峰值检测（`hal_max30100.cpp`）完全不变
- 实时 BPM 计算不变
- RR 间期记录和缓冲区不变

---

## 二、峰值检测改进尝试与撤回（已撤回）

### 为什么试

最直接的思路是在源头消除假拍——在峰值检测层加空白窗口挡住重搏切迹。

### 尝试了什么

**文件**：`hal_max30100.cpp`（已完全恢复原样）

1. 新增 `blanking` 变量（心跳后 300ms 禁止检测）
2. 不应期从 300ms → 400ms
3. 心跳后重置 peak/trough 为当前值，为新周期重新跟踪
4. peak/trough 改为始终更新（不应期内不跳过）

### 为什么撤回

烧录后 BPM 从正常 55–60 掉到 **37**。blanking 300ms + 不应期 400ms 的组合太激进，部分真实心跳也被挡住。原因可能是用户 PPG 波形的收缩峰上升支较缓，blanking 结束后阈值已漂移。

### 教训

峰值检测直接影响实时心率——这是用户最关心的指标。在 HRV 层做过滤是更安全的策略：风险隔离，BPM 计算完全不受影响。

---

## 三、呼吸引导放松训练

### 为什么改

重度压力（Level 3）检测到后应主动引导用户进行呼吸放松，而非仅显示文字。需要一个沉浸式的呼吸动画，配合 3 秒呼气 / 3 秒吸气的节奏。

### 需求

- Lv3 检测到 → 自动触发
- 3 秒呼气（圆环从中心向外扩散）+ 3 秒吸气（圆环从周围向内收缩）
- 蓝色圆环动画，呼气扩散、吸气收缩
- 8 个同心圆环，外层粗内层细
- 连续性强，无缝循环

### 迭代过程

**v1**：离散状态机 Phase 1(3s 呼气) ↔ Phase 2(3s 吸气)，每次切换重置 `breathPhaseStart`。两个环（主环亮蓝 2px + 尾迹暗蓝）。

- **问题**：收缩切扩散时闪一个大圈（`prevRingR` 残留值用于擦除，新 phase 第一帧擦错位置）
- **问题**：只有两个环，视觉单薄

**v2**：连续三角波替代离散状态机。`cycleT = elapsed % 6000`，前 3000ms 扩散、后 3000ms 收缩，无缝循环。6 个同心圆环。

- **问题**：用户要求 8 个环、外层粗内层细

**v3（最终版）**：8 个同心环，间距 12px，线宽从外到内递减：

| 环索引 | 线宽 | 颜色 |
|--------|------|------|
| 0 | 5px | 亮蓝 (0x059F) |
| 1 | 4px | 暗蓝 (0x0317) |
| 2 | 3px | 暗蓝 |
| 3 | 2px | 暗蓝 |
| 4–7 | 1px | 暗蓝 |

### 最终改了什么

**文件**：`hal_display.h`、`hal_display.cpp`

- 新增颜色：`COLOR_BREATH_BLUE`（亮蓝 0x059F）、`COLOR_BREATH_DIM`（暗蓝 0x0317）
- 新增函数：`display_fillCircle()`、`display_drawCircle()`（封装 TFT_eSPI 的 fillCircle/drawCircle）

**文件**：`ui_screens.h`

- 新增枚举值：`VIEW_BREATHING`
- 新增接口：`ui_initBreathing()`、`ui_showBreathing()`（返回已完成循环数）、`ui_isBreathingDone()`

**文件**：`ui_screens.cpp`

- 新增呼吸状态机变量（`breathPhase`、`breathPhaseStart`、`breathCycles`、`prevR`、`breathNeedClear`）
- 动画参数宏定义（`BREATH_INTRO_MS=2000`、`BREATH_PHASE_MS=3000`、`BREATH_CYCLE_MS=6000`、`BREATH_MAX_R=98`、`BREATH_NUM_RINGS=8`、`BREATH_RING_GAP=12`）
- `drawRingsMonochrome()`：用单一颜色画全部环（擦除用黑色）
- `drawRingsColored()`：领头亮蓝 + 尾迹暗蓝
- `ui_showBreathing()`：完整动画渲染，三角波驱动半径，首帧全屏清除后逐帧擦旧画新

**文件**：`smart_health_1.ino`

- 新增变量：`breathingActive`、`breathingStartMs`
- Lv3 检测触发呼吸（每秒压力分类中检查）
- 呼吸完成/按键退出后显示「Take a Walk!」提示
- 呼吸动画独立于 `needRefresh` 每 50ms 刷新

### 呼吸动画参数总览

| 参数 | 值 |
|------|-----|
| 提示阶段 | 2s（"High Stress!" + "Start Breathing..."） |
| 呼气/吸气各 | 3s |
| 完整周期 | 6s |
| 循环次数 | 5（总计约 30s + 2s 提示） |
| 圆环数 | 8 个同心环 |
| 环间距 | 12px |
| 最大半径 | 98px（留 22px 边距） |
| 线宽 | 5→4→3→2→1→1→1→1 px（外→内） |
| 主色 | 亮蓝 0x059F |
| 尾迹 | 暗蓝 0x0317 |
| 刷新策略 | 首帧全屏清除 + 后续帧增量擦除（只擦旧环画新环，~3ms SPI 时间） |
| 退出方式 | 5 循环自动 / 短按按键手动 |

---

## 四、临时提示屏

### 为什么改

- 呼吸训练完成后应提示用户起身活动
- Lv2 轻度压力检测到应提醒休息冥想
- 需自动计时退出

### 改了什么

**文件**：`ui_screens.h`

- 新增枚举值：`VIEW_PROMPT`
- 新增常量：`PROMPT_LV3_DONE (0)`、`PROMPT_LV2_ALERT (1)`
- 新增函数：`ui_showPrompt(int type)`

**文件**：`ui_screens.cpp`

- `ui_showPrompt()`：
  - `PROMPT_LV3_DONE`：黄色 "Take a Walk!" + "Stand up & move around"
  - `PROMPT_LV2_ALERT`：橙色 "Stress a bit high" + "Rest 15 seconds / Meditate now"
  - 标题行用 `display_centerText`，正文两行用 `display_leftText` + 统一 x 坐标，实现视觉上整齐的左对齐文字块

**文件**：`smart_health_1.ino`

- 新增变量：`promptActive`、`promptStartMs`、`promptType`、`lv2CooldownUntil`
- `PROMPT_DURATION = 3000ms`，时间到自动回到监控界面
- Lv2 冷却 60 秒（`LV2_COOLDOWN_MS`），防重复弹出
- 提示屏只画一次（静态内容），不每帧重绘

### 触发流程

```
Lv3 检测 ──→ 呼吸引导 (5循环 ~30s) ──→ PROMPT_LV3_DONE (3s) ──→ 监控
Lv2 检测 ──→ PROMPT_LV2_ALERT (3s, 60s冷却) ──→ 监控
```

---

## 五、压力等级阈值校准

### 为什么改

心率 >100 也很难到 Lv3。原因：

1. WESAD 模型用**手腕 PPG**（Empatica E4）训练，手腕信号噪声大
2. 本设备手指 PPG 信号更干净，HRV 指标更稳定，sigmoid 输出更集中
3. 运动心率升高 ≠ 心理压力，delta_meanHR 贡献大但其他 6 个 delta 不完全同步，概率上不去
4. 原阈值 0.25/0.50/0.75 对手腕数据合适，对手指数据偏高

### 改了什么

**文件**：`wesad_hrv_model.h`，`wesad_stress_level()` 函数

| 等级 | 旧阈值 | 新阈值 | 区间宽度变化 |
|------|--------|--------|-------------|
| Lv0 Relaxed | prob < 0.25 | prob < 0.25 | 不变 |
| Lv1 Normal | 0.25 ~ 0.50 | 0.25 ~ **0.40** | 收窄 |
| Lv2 Mild | 0.50 ~ 0.75 | 0.40 ~ **0.65** | 放宽 |
| Lv3 High | ≥ 0.75 | ≥ **0.65** | 门槛降低 |

Lv3 门槛从 0.75 降到 0.65；Lv2 区间扩宽，给更多机会触发轻度压力提醒。

---

## 六、屏2 HRV 详情页独立刷新通道

### 为什么改

屏2（dispMode==2）的 HRV 7 项指标详情页一直不刷新，有概率全是 `--`。

### 根因

屏2 的刷新藏在 `needRefresh` 条件里，和屏0（心率监测）、屏1（BPM+IR）共用同一套逻辑：
- 呼吸/提示期间 `needRefresh` 被 `!breathingActive && !promptActive` 跳过
- `lastHrvUpdateCount` 在跳过期间冻结，切回来后状态不一致
- 导致屏2错过多次更新

### 改了什么

**文件**：`smart_health_1.ino`

- 屏2 拥有独立的 `lastHrvRendered` 计数器，完全不受 `needRefresh` 影响
- 新增 HRV 诊断输出：每 5 秒打印 `[HRV] valid=X nn=XX rmssd=XX sdnn=XX`，便于排查数据不足问题

```cpp
// ★ HRV 详情页独立刷新通道
if (dispMode == 2 && !breathingActive && !promptActive
    && g_hrvUpdateCount != lastHrvRendered) {
    display_clear();
    ui_showHRVDetails(g_hrvLatest, hrv_engine_getBaseline(), g_stressLevel);
    lastHrvRendered = g_hrvUpdateCount;
}
```

屏0 和屏1 继续走原来的 `needRefresh` 逻辑（加上 `dispMode != 2` 条件避免冲突）。

---

## 七、传感器 FIFO 溢出修复及性能优化

### 为什么改

IR 数值卡住不变（显示 57053,777 一直重复），传感器完全不刷新。

### 根因

MAX30102 走 I2C，TFT 走 SPI，两者总线独立。问题不在硬件，在软件时序。

呼吸动画 v3 之前在主循环中每帧（20fps / 50ms）调用 `display_clear()`（即 `fillScreen`），画 240×240=57600 像素，SPI 耗时约 **23ms**。主循环时序：

```
[3ms 采样] [正常] [正常] [3ms 采样] [23ms fillScreen!! 阻塞  ] [采样跳过...]
                                                         ↑ 传感器 FIFO 溢出
```

MAX30102 的 FIFO 只有 32 个槽位，100Hz 采样下 320ms 就满。23ms 的 SPI 阻塞反复出现，累积导致 FIFO 来不及被读取，溢出后读数冻住。

### 改了什么

**文件**：`ui_screens.cpp`

- 呼吸动画首帧：一次 `display_clear()`（~23ms，仅一次可接受）
- 后续帧：**只擦旧环 + 画新环**（~3ms），`drawRingsMonochrome(prevR, COLOR_BG)` + `drawRingsColored(r)`
- SPI 时间从 23ms 降到 3ms — **降低 87%**

**文件**：`smart_health_1.ino`

- 提示屏改成只画一次（`static bool promptDrawn`），不每帧 `display_clear()` + 重绘
- 呼吸动画调用处移除 `display_clear()`，只依赖 `ui_showBreathing()` 内部的增量擦除

---

## 压力四级分类原理（参考）

以下为系统压力分类的完整数据流，便于理解改动上下文：

### 数据流

```
MAX30102 传感器
  │ 手指 PPG (100Hz)
  ▼
峰值检测 (hal_max30100.cpp)
  │ RR 间期 (ms)
  ▼
HRV 引擎 (hrv_engine.cpp)
  │ 7 项 HRV 指标 (RMSSD, SDNN, MeanHR, MedianNN, pNN50, CVNN, IQR NN)
  │
  ├─ 静息基线 (60s 采集, 一次性快照)
  │
  └─ 当前窗口 (60s 滑动窗口, 持续更新)
       │
       │  delta = 当前 - 基线 (7 个差值)
       ▼
WESAD 逻辑回归模型 (wesad_hrv_model.h)
  │ sigmoid 输出压力概率 (0.0 ~ 1.0)
  ▼
四等分离散化 (wesad_stress_level)
  │ 0=放松, 1=正常, 2=轻度压力, 3=高压力
  ▼
放松建议 (stress_classifier.cpp)
  │ 0=无需, 2=呼吸训练, 3=起身活动
```

### WESAD 模型系数

| 特征 | 训练集均值 | 训练集标准差 | 模型系数 |
|------|-----------|-------------|---------|
| delta_RMSSD | -0.95 | 66.97 | **-0.88** |
| delta_SDNN | -9.79 | 53.55 | **+2.91** |
| delta_MeanHR | +11.54 | 16.95 | **+3.00** |
| delta_MedianNN | -105.87 | 147.50 | -1.34 |
| delta_pNN50 | +2.27 | 19.57 | **+2.23** |
| delta_CVNN | +0.016 | 0.076 | -1.69 |
| delta_IQRNN | -17.63 | 110.29 | -1.97 |
| Intercept | — | — | +0.44 |

**Sigmoid 输出**：

```
logit = intercept + Σ(coef[i] × (delta[i] - mean[i]) / scale[i])
prob  = 1 / (1 + e^(-logit))
```

### 四分类（改动后）

```
prob < 0.25  →  0 = STRESS_RELAXED  (放松)
prob < 0.40  →  1 = STRESS_NORMAL   (正常)
prob < 0.65  →  2 = STRESS_MILD     (轻度压力)
prob ≥ 0.65  →  3 = STRESS_HIGH     (高压力)
```

### 关于 WESAD 模型与手指 PPG 的差异

WESAD 模型使用手腕 PPG（Empatica E4）训练，本设备使用手指 PPG（MAX30102）。差异分析：

- **信号质量**：手指 PPG >> 手腕 PPG（毛细血管密集、运动伪影少）
- **HRV 指标**：部位无关，只要 RR 间期准确，算出来应该一致
- **模型影响**：因为用的是 **delta 特征**（当前 − 自身基线），系统性偏差互相抵消，方向性判断仍然正确
- **概率校准**：干净的手指数据可能导致概率轻微偏移，阈值已据此下调

---

## 完整文件改动清单

| 文件 | 改动内容 | 状态 |
|------|---------|------|
| **hal_display.h** | 新增 `COLOR_BREATH_BLUE`、`COLOR_BREATH_DIM`；新增 `display_fillCircle()`、`display_drawCircle()` 声明 | ✅ 已应用 |
| **hal_display.cpp** | 实现 `display_fillCircle()` 和 `display_drawCircle()`（封装 TFT_eSPI） | ✅ 已应用 |
| **ui_screens.h** | 新增 `VIEW_BREATHING`、`VIEW_PROMPT` 枚举值；新增 `ui_initBreathing()`、`ui_showBreathing()`、`ui_isBreathingDone()`、`ui_showPrompt()` 声明；新增 `PROMPT_LV3_DONE` / `PROMPT_LV2_ALERT` 常量 | ✅ 已应用 |
| **ui_screens.cpp** | 新增呼吸动画状态机（连续三角波、8 同心环、5→4→3→2→1→1→1→1 px 线宽、增量擦除）；新增 `ui_showPrompt()` 提示屏渲染（手动左对齐文字块） | ✅ 已应用 |
| **hrv_engine.cpp** | 新增 `#include <math.h>`；`hrv_engine_compute()` 中新增 RR 中位数偏差 >30% 成对过滤逻辑；最低 NN 数从 30→20 | ✅ 已应用 |
| **wesad_hrv_model.h** | `wesad_stress_level()` 压力等级阈值从 0.25/0.50/0.75 → 0.25/0.40/0.65（对手指 PPG 校准） | ✅ 已应用 |
| **smart_health_1.ino** | 新增 `breathingActive` / `promptActive` 状态变量及触发逻辑；Lv3→呼吸引导 / Lv2→休息提醒 / Lv3完成→活动提示；屏2 独立刷新通道；HRV 诊断输出；性能优化（移除冗余 fillScreen） | ✅ 已应用 |
| **hal_max30100.cpp** | 峰值检测 blanking 改进 — **已完全撤回**，保持原始版本 | ⏪ 已撤回 |

### 未修改的文件

| 文件 | 说明 |
|------|------|
| `app_config.h` | 所有参数定义未变 |
| `dsp_ppg.h` / `.cpp` | 心率计算逻辑未变 |
| `hal_button.h` / `.cpp` | 按键逻辑未变 |
| `comm_uart.h` / `.cpp` | 通信逻辑未变 |
| `stress_classifier.h` / `.cpp` | 压力分类器未变（阈值改在 wesad_hrv_model.h 中） |
| `hrv_engine.h` | 接口未变（过滤逻辑在 .cpp 内部） |

---

## 系统行为总览（改动后）

### 开机流程

```
开机 → 传感器初始化 → 提示放置手指 → 60s 静息基线采集
    → "Calibration Complete!" (2s) → 正常监测模式
```

### 压力响应逻辑

```
压力分类 (每秒)：
  ├─ Lv3 High (prob ≥ 0.65)
  │    → 呼吸引导 (2s提示 + 5循环×6s动画)
  │    → "Take a Walk!" (3s)
  │    → 回到监控
  │
  ├─ Lv2 Mild (prob 0.40~0.65, 60s 冷却)
  │    → "Rest 15 seconds / Meditate" (3s)
  │    → 回到监控
  │
  └─ Lv0/Lv1 → 正常监控
```

### 呼吸动画参数

| 参数 | 值 |
|------|-----|
| 提示阶段 | 2s（"High Stress!" + "Start Breathing..."） |
| 呼气/吸气各 | 3s |
| 完整周期 | 6s |
| 循环次数 | 5（总计约 30s） |
| 圆环数 | 8 个同心环，间距 12px，最大半径 98px |
| 线宽 | 5→4→3→2→1→1→1→1 px（外→内） |
| 主色/尾迹 | 亮蓝 0x059F / 暗蓝 0x0317 |
| 刷新 | 20fps，首帧全屏清除后增量擦除（~3ms/帧） |
| 退出 | 5 循环自动 / 短按按键手动 |
