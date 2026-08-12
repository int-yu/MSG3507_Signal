# MSPM0G3507 API 调用教程

本教程的四个程序位于 [`examples/api_usage/`](../examples/api_usage/)；可与核心和可选后端一起由 GCC、TI Arm Clang 编译。实时程序只使用固定宽度整数、静态数组和库状态：不含浮点、`math.h` 或动态分配。配置工具或显示任务可以按 [API.md](API.md) 的公式换算物理量，但不得把换算带入采样 ISR。

建议阅读顺序：先看 [API.md](API.md) 的数值约定，再从预处理、测量、跟踪、后端四个完整程序按需复制。`sigq15_diag_t diag={0};` 是一帧/一段时间的诊断器；把 `saturation_count` 非零当作幅度、系数或状态缩放需要复核的信号。

## 1. ADC/DMA → 校准 → 滤波 → 抽取

完整程序：[preprocess_filter.c](../examples/api_usage/preprocess_filter.c)。示例在采集边界将 12 位中点 ADC 码转换为 Q1.15：`((int32_t)adc_code - 2048) << 4`。对于不同 ADC 分辨率、双极性编码和模拟前端，必须在板级采集适配层重新推导这一映射；显示层电压公式为 `volts = q15 * Vref / 32768`。

初始化阶段静态分配全部工作区并检查每个状态码：

```c
static int16_t fir_state[3];
sigq15_fir_t fir;
if (sigq15_fir_init(&fir, fir_coeffs, 3, fir_state, 3) != SIGQ15_OK) return 1;
```

实时顺序是 `linear_calibrate → dc_blocker → moving_average → biquad → decimator`。抽取器内部再次使用 FIR，输出容量必须不少于 `ceil(input_count/factor)`。新量程、丢 DMA 帧或切换滤波参数时，在安全边界调用 reset，不要写结构体内部字段。若 `saturation_count` 增加，先降低前端增益或重新缩放系数；若 invalid 计数增加，检查是否 init 失败后仍进入 ISR。移动平均初始 length 个样点有预热衰减，上层可等待 `filled==length`。

## 2. 帧测量 → 窗 → 单频谱与质量指标

完整程序：[measure_spectrum.c](../examples/api_usage/measure_spectrum.c)。生产系统中先窗后 Goertzel：

```c
sigq15_apply_window(frame, windowed, 256, SIGQ15_WINDOW_HANN, &diag);
if (sigq15_goertzel(windowed, 256, 1600, 100, &tone, &diag) != SIGQ15_OK) return 1;
```

`sigq15_stats` 的 RMS 是 Q1.15；显示归一化 RMS 为 `rms_q15/32768`。过零 API 返回 mHz/Q16.16 周期，只有 status OK 且 valid 才上报；threshold 要高于噪声。Goertzel 目标在 `(0, Fs/2)`，非相干帧按 coherent gain 修正显示幅度。谱指标的 THD/THD+N 是 Q1.15 比率，SNR/SINAD/SFDR 是 Q8.8 dB，显示端除 256；相关 delay 除 65536 得样点延时。

幅度、THD、THD+N、传递函数保真路径**不得**使用 AGC；时变增益会破坏幅度和谐波。扫频每个驻留点调用 `sigq15_transfer_point` 并丢弃变频过渡帧。

## 3. 相位连续载波 → IQ/FLL/PLL → NLMS

完整程序：[tracking_adaptive.c](../examples/api_usage/tracking_adaptive.c)。NCO 用 Q0.32 相位，set frequency 保留 phase；表为 Q1.15 且长度是 >=4 的二次幂。IQ alpha 小则响应慢、二倍频抑制更好；I/Q/幅度 Q1.15、相位 Q0.32。FLL 输入有符号 Q0.32 误差、输出 mHz；PLL kp/ki 为 Q2.30，必须以 `sigq31_pll_locked` 判断。掉信号时冻结最后可信频率，恢复后用 FLL mHz reset PLL。

`normalized=1` 选择 NLMS；输入、权重、error 均 Q1.15。error/饱和持续上升时降低 mu 并 reset。示例中的 AGC 只在显示支路，绝不馈入 IQ 幅度、THD 或 transfer-point。

## 4. 可选 CMSIS/MATHACL 后端

完整程序：[platform_backends.c](../examples/api_usage/platform_backends.c)。先检查可用性，CMSIS 可用才调用 RFFT：

```c
if (sigq15_cmsis_available()) status = sigq15_cmsis_rfft_q15(input, 256, packed);
```

CMSIS RFFT 仅 256/512 点 Q1.15，构建时由项目提供 CMSIS-DSP include/library 和 `SIGQ15_USE_CMSIS_DSP`。没有该宏时 API 存在但不可用并返回 ERANGE。CMSIS 块缩放、链接和目标数值须由项目验证。MATHACL 可用标志只表示编译适配层，不能说明寄存器事务、并发规则或性能已在板上验证。

## 编译与上线检查

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Iinclude src/sigq15.c backends/sigq15_backends.c examples/api_usage/preprocess_filter.c -o preprocess_filter
python -m pytest tests/test_api_docs.py -q
```

TI Arm Clang 命令与 SDK 约束见 [TI_BUILD.md](TI_BUILD.md)。文档契约会从公开头提取符号，检查参考/清单/链接，编译四个程序，并拒绝实时文件中的浮点、math.h 或动态分配。
