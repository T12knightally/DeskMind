/*
 * app_config.h - 全局配置参数
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ========== 采样配置 ==========
#define SAMPLE_RATE_HZ      100     // PPG 采样率 (Hz)
#define SAMPLE_INTERVAL_MS  (1000 / SAMPLE_RATE_HZ)  // 10ms

// ========== BPM 计算参数 ==========
#define BPM_SAMPLE_WINDOW   4       // 用于BPM平滑的平均拍数
#define BPM_MIN             30      // 最低有效心率
#define BPM_MAX             220     // 最高有效心率
#define BEAT_TIMEOUT_MS     5000    // 超过5秒无心跳则认为传感器脱落

// ========== DC 滤波参数 (一阶高通 IIR) ==========
// 截止频率 ~0.5Hz, 采样率100Hz -> alpha ≈ 0.969
// y[n] = alpha * (y[n-1] + x[n] - x[n-1])
#define FILTER_ALPHA        0.969f

// ========== 峰值检测参数 ==========
#define PEAK_WINDOW_SIZE    10      // 用于阈值计算的采样窗口数

// ========== HRV 分析参数 ==========
#define RR_BUFFER_SIZE      128     // RR间期缓冲区大小 (约2分钟数据)
#define HRV_WINDOW_SEC      60      // HRV分析窗口 (秒)
#define HRV_UPDATE_INTERVAL 10000   // HRV计算/发送间隔 (ms, 每10秒)
#define BASELINE_CAPTURE_SEC 60     // 静息基线采集时长 (秒)

// ========== 通信参数 ==========
#define COMM_BAUDRATE       115200  // UART 波特率
#define COMM_SEND_BPM_INTERVAL 1000 // BPM数据发送间隔 (ms)
#define COMM_SEND_HRV_INTERVAL HRV_UPDATE_INTERVAL  // HRV特征发送间隔
#define COMM_RX_TIMEOUT     5000    // 队友通信超时(ms)

// ========== 压力等级映射 ==========
#define STRESS_RELAXED      0
#define STRESS_NORMAL       1
#define STRESS_MILD         2
#define STRESS_HIGH         3

// ========== 放松方式映射 ==========
#define RELAX_NONE          0
#define RELAX_REST          1
#define RELAX_BREATHING     2
#define RELAX_MOVE          3

// ========== 显示配置 ==========
#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       240
#define UI_REFRESH_MS       50      // 屏幕刷新间隔 (20fps)

// ========== 按键配置 ==========
#define DEBOUNCE_MS         50      // 去抖时间
#define LONG_PRESS_MS       1000    // 长按阈值

#endif
