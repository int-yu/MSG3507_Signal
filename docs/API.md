# MSPM0G3507 定点信号 API 参考

本页是 `include/sigq15.h` 与 `include/sigq15_backends.h` 的逐符号说明；可运行的整型调用顺序见 [API_USAGE.md](API_USAGE.md)。库不分配动态内存，所有状态和工作区须由调用者静态分配，并在采样中断前完成 `*_init`。

## 统一数值、状态与诊断

`int16_t` 信号和大多数系数为 **Q1.15**：归一化物理值 = `q15 / 32768`，范围 `[-1, 32767/32768]`。显示层电压可算 `q15 * Vref / 32768`，但实时 C 不做此换算。`uint32_t` 相位是无符号 **Q0.32** 一圈：角度 = `phase * 360 / 2^32`。Q16.16 样点时间 = `value / 65536` 样点；Q8.8 dB = `value / 256 dB`；Q12.20（Q20）传函 = `value / 1048576`；Q2.30 = `value / 1073741824`。Q15 输出夹到 `[-32768,32767]`，Q31 输出夹到 `INT32_MIN..INT32_MAX`。

状态码：`OK` 成功；`EINVAL` 参数/空指针非法；`EWORKSPACE` 工作区不足；`ENOSIGNAL` 无有效信号；`ENOTLOCKED`/`ENOTCONVERGED` 供上层状态机使用；`ERANGE` 参数范围或可选后端不支持。每个结果的 `valid` 都须先检查。`sigq15_diag_t` 的 `saturation_count` 是夹位次数、`overflow_count` 供上层累计溢出、`invalid_count` 是无效逐点调用；`valid/locked/converged` 是帧级标志。每测量帧前由调用者清零；API 不清累计诊断。工作区只要状态对象仍使用就必须有效；`*_reset` 保留配置和工作区地址而清空历史。

## 标量、相位与标定

| API（精确原型） | Q 格式、范围、诊断、错误/复位 |
|---|---|
| `int16_t sigq15_sat(int32_t value, sigq15_diag_t *diag);` | 整数夹为 Q1.15 原始码；超界递增 `saturation_count`；无状态/复位。 |
| `int32_t sigq31_sat(int64_t value, sigq15_diag_t *diag);` | 宽累加夹为 Q31 原始码；超界递增诊断；无状态。 |
| `uint32_t sigq31_phase_wrap(uint32_t phase);` | Q0.32 天然模 (2^32) 回绕，原样返回；无诊断。 |
| `uint32_t sigq31_phase_to_millidegrees(uint32_t phase);` | Q0.32 转 `0..359999` mdeg；显示角度为 `mdeg/1000`。 |
| `int16_t sigq15_median3(int16_t a,int16_t b,int16_t c);` | 三个 Q1.15 的中值，适合单点毛刺；无状态/诊断。 |
| `int16_t sigq15_linear_calibrate(int16_t sample,int16_t gain_q15,int16_t offset_q15,sigq15_diag_t *diag);` | `y=round(sample*gain/2^15)+offset`；三项 Q1.15；输出饱和记诊断。 |

最小调用：`int16_t corrected = sigq15_linear_calibrate(adc_q15, gain_q15, offset_q15, &diag);`。先在采集边界将 ADC 满量程码映射/左移为 Q1.15，不能把原 ADC 码直接当 Q1.15。

## 去偏置、平均、FIR、抽取和 SOS

`sigq15_dc_blocker_t { alpha_q15,x1,y1 }` 使用 `y[n]=x[n]-x[n-1]+alpha*y[n-1]`；alpha 为 `[0,32767]` Q1.15，越近 1 截止越低。移动平均 `state[length]` 是 Q1.15 环形历史、`sum` 是 Q63 累加，预热也除完整 length。FIR 系数/历史为 Q1.15，64 位累加后回 Q1.15。SOS `b0,b1,b2,a1,a2` 为 Q1.15、DF2T 方程保存减号的 a1/a2，`post_shift` 为 `0..14`，`d1/d2` 为 Q31 状态；FIR/SOS 输出饱和记诊断。

| API（精确原型） | Q 格式、工作区、复位、错误 |
|---|---|
| `sigq15_dc_blocker_init(state,alpha_q15)` / `sigq15_dc_blocker_reset(state)` / `sigq15_dc_blocker_process(state,input,diag)` | input/output Q1.15；init 要非空、alpha 非负，否则 `EINVAL`；process 成功设 `diag->valid`，空 state 返回 0 并记 invalid。 |
| `void sigq15_remove_mean(int16_t *samples,size_t count);` | 原地减 Q1.15 均值；无工作区/诊断，空或零长度无操作；只处理完整、非 DMA 写入帧。 |
| `size_t sigq15_moving_average_workspace_size(size_t length);` / `sigq15_moving_average_init(filter,state,length,state_count)` / `sigq15_moving_average_reset(filter)` / `sigq15_moving_average_process(filter,input,diag)` | 工作区 `length*sizeof(int16_t)`；`state_count>=length`，否则 `EWORKSPACE`；输入输出 Q1.15，空逐点状态记 invalid。 |
| `size_t sigq15_fir_workspace_size(size_t taps);` / `sigq15_fir_init(fir,coeffs,taps,state,state_count)` / `sigq15_fir_reset(fir)` / `sigq15_fir_process(fir,input,diag)` / `sigq15_fir_process_block(fir,input,output,count,diag)` | 工作区 `taps*sizeof(int16_t)`；系数/状态 Q1.15；块 API 等价按序逐点，成功设 valid，非法参数 `EINVAL`。 |
| `sigq15_decimator_init(decimator,anti_alias_fir,factor)` / `sigq15_decimator_reset(decimator)` / `size_t sigq15_decimator_process(decimator,input,input_count,output,output_capacity,diag)` | factor >=2；先抗混叠 FIR、每 factor 点输出。返回实际写入数；容量不足仅丢额外输出，调用者至少留 `ceil(input_count/factor)`；reset 也复位 FIR。 |
| `size_t sigq15_biquad_workspace_size(size_t stages);` / `sigq15_biquad_init(filter,coeffs,stages,state,state_count)` / `sigq15_biquad_reset(filter)` / `sigq15_biquad_process(filter,input,diag)` | 工作区 `stages*sizeof(sigq15_biquad_state_t)`；状态数不足 `EWORKSPACE`，post_shift 越界 `ERANGE`；输出 Q1.15。 |

## 时域测量与相关

| API（精确原型） | Q 格式、范围与失效 |
|---|---|
| `sigq15_stats(input,count,result)` | `mean_q15/rms_q15/minimum_q15/maximum_q15/peak_q15` 均 Q1.15；RMS 是 `sqrt(mean(x^2))`。全零 `ENOSIGNAL`、valid=0；否则 OK。 |
| `sigq15_frequency_zero_cross(input,count,sample_rate_hz,threshold_q15,result)` | `frequency_millihz` 为 mHz，`period_q16_samples` 为 Q16.16，crossings 是计数。只计上升过零且斜率 >= Q1.15 threshold；少于两次 `ENOSIGNAL`。 |
| `sigq15_cross_correlate(a,b,count,max_lag,result)` | `correlation_q30` 为饱和乘积累加，`delay_q16_samples` 为 Q16.16（正值是 b 的峰较后），phase_q32 预留为 0；零相关 `ENOSIGNAL`。 |

调用：`if (sigq15_frequency_zero_cross(frame,N,FS,threshold,&f)==SIGQ15_OK && f.valid) display_mhz=f.frequency_millihz;`。threshold 应高于噪声和若干 ADC LSB。

## 窗、Goertzel、谱指标与传函

窗枚举为 `SIGQ15_WINDOW_RECT/HANN/BLACKMAN_HARRIS/FLAT_TOP`。窗值、加窗输出、coherent gain 均 Q1.15；ENBW 为 Q12（`enbw_q12/4096` 个 bin）。非相干采样先加窗，并按 coherent gain 校正幅度。

| API（精确原型） | Q 格式、范围、诊断与错误 |
|---|---|
| `sigq15_window_value(window,index,count)` / `sigq15_apply_window(input,output,count,window,diag)` / `sigq15_window_coherent_gain_q15(window)` / `sigq15_window_enbw_q12(window)` | 输入/输出 Q1.15；apply 饱和记诊断；无工作区/状态；count<=1 窗值为满量程。 |
| `sigq15_goertzel(input,count,sample_rate_hz,target_hz,result,diag)` / `sigq15_goertzel_multi(input,count,sample_rate_hz,targets_hz,results,target_count,diag)` | target 满足 `0<target<sample_rate/2`；real/imag 是缩放 Q1.15，power_q30 是 Q30，amplitude_q15 是 Q1.15，phase_q32 一圈。非法 `EINVAL`，零信号 `ENOSIGNAL`，夹位记诊断。递推 `2*cos(w)` 是 **Q2.14**，不可按 Q1.15 量化。 |
| `sigq15_spectrum_metrics(power_q30,bins,fundamental_bin,harmonic_count,result)` | 输入 bin 功率为无符号 Q30；THD/THD+N 是 Q1.15 比率；SNR/SINAD/SFDR 是有符号 Q8.8 dB；noise_power_q30 饱和 32 位。基本波索引 1..bins-1 且非零，否则 `EINVAL`。 |
| `sigq15_transfer_point(input,output,count,sample_rate_hz,frequency_hz,result,diag)` | 同频 Y/X；real_q20/imag_q20/magnitude_q20 是 Q12.20，phase_q32 一圈，coherence_q15 当前 32767。无信号 `ENOSIGNAL`；扫频变点后丢弃过渡帧。 |

幅度、THD、THD+N、transfer-function 保真路径**不得使用 AGC**，否则时变 gain 会破坏物理幅度和谐波。

## NCO、AGC、IQ、LMS、FLL 与 PLL

| API（精确原型） | Q 格式、生命周期、错误/诊断 |
|---|---|
| `sigq31_nco_init(nco,sample_rate_hz,frequency_hz,sine_table,table_size)` / `sigq31_nco_set_frequency(nco,sample_rate_hz,frequency_hz)` / `sigq31_nco_next(nco,sine,cosine)` | phase/step Q0.32；表 Q1.15，长度必须 >=4 的二次幂；频率小于采样率。set 不清 phase，保证相位连续；非法 EINVAL、越范围 ERANGE。 |
| `sigq15_agc_init(agc,target_q15,attack_q15,release_q15,min_gain_q15,max_gain_q15)` / `sigq15_agc_reset(agc)` / `sigq15_agc_process(agc,input,diag)` | target/attack/release Q1.15；gain 字段名 q15 但语义为 **Q2.14**，实际增益 `gain_q15/16384`。范围正且 min<=max；输出 Q1.15 饱和记诊断。仅限防削顶/显示支路。 |
| `sigq15_iq_init(demod,sample_rate_hz,carrier_hz,alpha_q15,sine_table,table_size)` / `sigq15_iq_reset(demod)` / `sigq15_iq_process(demod,sample,diag)` | 内部 I/Q 是 Q2.30；结果 i/q/amplitude Q1.15、phase Q0.32。alpha 小则响应慢而二倍频抑制好；init 继承 NCO 参数错误。 |
| `size_t sigq15_lms_workspace_size(taps)` / `sigq15_lms_init(lms,weights,history,taps,mu_q15,normalized)` / `sigq15_lms_reset(lms)` / `sigq15_lms_process(lms,reference,desired,error,diag)` | weights/history/reference/desired/error 全 Q1.15；函数工作区值是单组 bytes，两组均须 taps 个 int16。mu 必须正；normalized=0 LMS、1 NLMS；发散时减 mu 并读饱和计数。 |
| `sigq31_fll_init(fll,sample_rate_hz,initial_millihz,min_millihz,max_millihz,gain_q15)` / `sigq31_fll_update(fll,phase_error_q32)` | 频率均 mHz、gain Q1.15、误差为有符号 Q0.32；频率夹 min/max，非法 EINVAL。FLL 用于捕获，掉信号由上层冻结/重置。 |
| `sigq31_pll_init(pll,sample_rate_hz,initial_millihz,kp_q30,ki_q30,min_millihz,max_millihz)` / `sigq31_pll_reset(pll,frequency_millihz)` / `sigq31_pll_process(pll,sample,sine_table,table_size)` / `sigq31_pll_locked(pll)` | kp/ki 与 integrator 为 Q2.30，频率 mHz、phase Q0.32；表规则同 NCO。连续低误差超过 100 点 locked 才为 1；无效表 process 返回 0。 |

## 可选后端

| API（精确原型） | 可移植约定和限制 |
|---|---|
| `sigq15_backend_available(void)` | 返回 PORTABLE/CMSIS/MATHACL 编译时选择；核心不要求 SDK。 |
| `sigq15_cmsis_available(void)` / `sigq15_cmsis_rfft_q15(input,length,packed_output)` | 仅定义 `SIGQ15_USE_CMSIS_DSP` 且链接 CMSIS-DSP 时可用；仅 256/512 点，输入/输出 Q1.15。不可用时 available=0、RFFT 为 ERANGE；块缩放需应用自行验证。 |
| `sigq15_mathacl_available(void)` / `sigq15_backend_sqrt_q30(value_q30)` | Q30 非负量的整数平方根；标志仅说明编译开关，**不代表已验证 MATHACL 事务或目标性能**。 |

init 失败后不得 process。不要手改 ABI 状态字段；用 reset 清历史。完整静态缓冲、返回值与掉锁处理见 [API_USAGE.md](API_USAGE.md)。

## 公开结构体和字段生命周期

以下字段是 ABI 的可见状态，应用可读取结果/诊断，但不应在运行中手改滤波索引、累加器或历史。`sigq15_diag_t`：`saturation_count/overflow_count/invalid_count` 为累计计数，`valid/locked/converged` 为 uint8 帧级标志。`sigq15_dc_blocker_t`：`alpha_q15` 为 Q1.15 配置，`x1/y1` 为上一输入/输出 Q1.15。`sigq15_moving_average_t`：`state` 指向调用者 Q1.15 工作区，`length` 为窗口，`index` 为写位置，`filled` 为已填样本数，`sum` 为 int64 累加。`sigq15_fir_t`：`coeffs/state` 分别指向 Q1.15 常量和工作区，`taps/index` 为长度/环形位置。`sigq15_decimator_t`：`fir` 指向已初始化抗混叠 FIR，`factor/phase` 为抽取因子/当前相位。

`sigq15_biquad_coeffs_t` 的 `b0/b1/b2/a1/a2` 为 Q1.15，`post_shift` 是每级缩放；`sigq15_biquad_state_t.d1/d2` 是 Q31；`sigq15_biquad_t.coeffs/state/stages` 分别是配置、工作区和级数。`sigq15_stats_result_t` 的全部测量字段为 Q1.15，`valid` 说明非零有效。`sigq15_frequency_result_t.frequency_millihz`、`center_millihz` 等频率字段使用 mHz，`period_q16_samples` 是 Q16.16，`crossings` 是次数。`sigq15_correlation_result_t.correlation_q30` 为 Q30、`delay_q16_samples` 为 Q16.16、`phase_q32` 为 Q0.32、`valid` 为有效标志。

`sigq15_goertzel_result_t.real_q15/imag_q15/amplitude_q15` 是 Q1.15，`power_q30` 是 Q30，`phase_q32` 是 Q0.32。`sigq31_nco_t.phase/step` 是 Q0.32，`sine_table/table_size` 描述只读 Q1.15 表。`sigq15_agc_t.target_q15/attack_q15/release_q15` 是 Q1.15，而 `min_gain_q15/max_gain_q15/gain_q15` 按 Q2.14 解释。`sigq15_iq_demod_t.nco` 内嵌载波，`alpha_q15` 是 Q1.15，`i_q30/q_q30` 是 Q2.30；`sigq15_iq_result_t.i_q15/q_q15/amplitude_q15` 为 Q1.15。

`sigq15_lms_t.weights/history` 指向 Q1.15 静态数组，`taps/index` 为维度/位置，`mu_q15` 是 Q1.15 步长，`normalized` 为 0（LMS）或 1（NLMS）。`sigq31_fll_t.sample_rate_hz` 是整数 Hz，`frequency_millihz/center_millihz/min_millihz/max_millihz` 为 mHz，`gain_q15` 为 Q1.15。`sigq31_pll_t.phase` 为 Q0.32，`frequency_millihz` 及限值为 mHz，`kp_q30/ki_q30` 和 `integrator_q30` 为 Q2.30，`lock_metric/lock_count` 是锁定状态，`previous_sample` 为 Q1.15，`samples_since_crossing` 是计数。

`sigq15_transfer_result_t.real_q20/imag_q20/magnitude_q20` 为 Q12.20，`phase_q32` 为 Q0.32，`coherence_q15` 为 Q1.15。`sigq15_spectrum_metrics_t.thd_q15/thdn_q15` 为 Q1.15，`snr_q8_db/sinad_q8_db/sfdr_q8_db` 为 Q8.8 dB，`noise_power_q30` 为 Q30，`valid` 为有效性。所有 result 在对应 status 为 OK 且 valid 为 1 前都不可当作测量值。
