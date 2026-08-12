# MSPM0G3507 Signal

面向 TI 杯模拟电子系统设计邀请赛的 MSPM0G3507 纯定点信号处理库。仓库沿用 MSG3507_Signal 名称，实际目标器件是 MSPM0G3507（Cortex-M0+）。

首版只提供算法与平台适配接口，不配置 ADC、DAC、DMA、比较器或具体 GPIO。实时源使用 Q15/Q31/Q63 整数，不使用动态内存或软件浮点。

## 能力

- 饱和、校准、去均值、中值、DC blocker、统计、RMS
- FIR、滑动平均、SOS biquad、FIR 抗混叠抽取
- 插值过零频率、互相关 Q16.16 亚采样时延（三点抛物线插值）
- Hann、Blackman-Harris、Flat-top、Goertzel、多频点和频谱指标
- 32位 NCO、相位连续改频、I/Q 解调
- LMS/NLMS、二阶 PLL、单点复数传递函数
- 可选 CMSIS-DSP RFFT 声明和 MATHACL 隔离层

详见 docs/API.md 和 docs/WIRING.md。

## 验证

~~~powershell
powershell -ExecutionPolicy Bypass -File tests/run_host.ps1
python -m pytest -q
~~~

TI Arm Clang 目标语法命令见 docs/TI_BUILD.md。

## CCS / SysConfig

依赖 MSPM0 SDK 2.10.00.04、兼容 SysConfig 和 TI Arm Clang。导入 MSG3507_Signal.projectspec。工程只带空闲主程序与算法源；根据自制板原理图完成外设和 pin map 后，再把 DMA 缓冲区交给算法。

## 边界

- 1 Hz–100 kHz 还要求采样率满足 Nyquist；模拟前端、ADC与时钟误差不计入算法指标。
- Q15 的 1.0 表示为 32767；状态和工作区由调用者提供。
- Goertzel 系数是 2*cos(w) 的 Q2.14，不可按普通 Q1.15 量化。
- AGC 会破坏真实幅值，未接入精密测量链。
- CMSIS 后端定义 SIGQ15_USE_CMSIS_DSP；MATHACL 后端须真机补齐并验证事务。

MIT License。
## 详细 API 手册

推荐先读 [API 参考](docs/API.md)，再按 [固定点调用教程](docs/API_USAGE.md) 的四个可编译示例实践：预处理、测量频域、跟踪自适应与可选后端。实时例程只用静态工作区和定点整数；硬件边界见 [接线说明](docs/WIRING.md)。
