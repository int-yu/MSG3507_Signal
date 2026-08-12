# MSPM0G3507 API 调用手册扩建设计

## 目标

把现有接口摘要扩展为可直接照抄、编译并接入 MSPM0G3507 采样链的中文手册。`include/sigq15.h` 与 `include/sigq15_backends.h` 是公开接口的唯一事实来源，不改变算法 ABI。

## 文档结构

1. `docs/API.md`：逐个公开函数和结构体字段的权威参考。
2. `docs/API_USAGE.md`：按竞赛任务组织完整调用流程。
3. `examples/api_usage/`：与教程对应的可编译纯定点 C 示例。

每个 API 条目固定包含：原型、用途、参数、Q 格式/物理单位/范围、工作区和状态生命周期、返回码、诊断字段、结果换算、调用位置、最小示例和常见错误。

## 场景教程

- ADC 原始码转 Q15、线性校准、去直流和饱和诊断。
- FIR、SOS IIR、移动平均和抗混叠抽取。
- RMS、过零频率、幅值/相位和 Q16.16 亚采样时延。
- 窗函数、Q2.14 Goertzel、CMSIS Q15 RFFT 和 Q8.8 dB 指标。
- 32 位相位 NCO、I/Q 解调、PLL/FLL 与掉信号处理。
- LMS/NLMS、AGC 的适用边界和 50 Hz 抵消。
- 扫频复数传递函数。
- CMSIS-DSP/MATHACL 条件后端启用方法。

所有实时示例禁止 `float`、`double`、标准 `math.h` 和动态分配。文档必须明确 AGC 禁止进入幅值、THD 和传递函数保真链。硬件章节只写逻辑采样链，不虚构封装脚号。

## 验证

- 从两个公开头文件提取符号，与 API 文档覆盖清单核对。
- `examples/api_usage/*.c` 使用 GCC 和 TI Arm Clang 5.1.1.LTS 严格编译。
- 扫描实时示例与库源码，禁止浮点、标准数学库和动态内存。
- Markdown、UTF-8、链接与占位符扫描通过。

## 非目标

不修改算法实现和公开函数签名；不声称未完成的 MATHACL 真机寄存器事务、CMSIS-DSP 真机链接或周期/RAM benchmark；不指定未知板级引脚。
