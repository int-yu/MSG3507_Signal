# 逻辑接线模板

只定义逻辑角色，不指定封装脚号。原理图定稿后新增 pin_map.md，算法 API 不变。

| 角色 | 电气要求 | 用途 |
|---|---|---|
| SIGNAL_ADC | 保护、偏置、抗混叠后进入 ADC | 幅值、频谱、滤波 |
| REFERENCE_ADC | 与主路同步，群延迟可标定 | 相位、传递函数、相干解调 |
| ZERO_CROSS | 比较器输出，阈值和迟滞明确 | 定时器捕获粗频率 |
| DAC_OUT | 单路 DAC 后接重建滤波和缓冲 | 未来 DDS/扫频 |
| SYNC_OUT | 可选定时器/GPIO 脉冲 | 仪器触发 |

ADC 不得超出参考范围；双极信号需加中点偏置；满量程留 10%–20% 余量。抗混叠截止必须低于 Nyquist，抽取前还需数字 FIR。双路相位测量必须共用触发；非同时采样 ADC 要校准时差。MSPM0G3507 只有一路 DAC，不按硬件双路 I/Q 规划。

未来数据流：Timer -> ADC trigger -> DMA ping/pong -> algorithm block -> result queue。算法只接收数组和采样率，不直接操作寄存器。