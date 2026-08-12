# 接口与算法手册

## 约定

sigq15_ 表示 Q1.15，sigq31_ 表示 32 位量或相位。相位一圈为 [0,2^32)，0x40000000 是 90°。返回值是 sigq15_status_t；输出 valid 表示可用；sigq15_diag_t 累积饱和、溢出和非法调用。库不分配内存，工作区全部由调用者提供。

## 预处理与滤波

- linear_calibrate：y=x*gain+offset，系数为 Q15。
- dc_blocker：y[n]=x[n]-x[n-1]+alpha*y[n-1]。
- remove_mean、中值去毛刺、滑动平均。
- FIR：Q15 系数，64 位累加；decimator 每点先抗混叠再每 M 点输出。
- biquad：SOS DF2T，结构为 b0,b1,b2,a1,a2,post_shift。a1/a2 按差分方程的减号保存。用 design_iir.py 生成并检查极点和缩放。

## 测量与频域

- stats：均值、RMS、极值和峰值。
- frequency_zero_cross：正向过零，Q16 线性插值，至少两个过零。
- cross_correlate：整数 lag 搜索；亚样点字段已预留，首版小数为零。
- window：纯整数 CORDIC；相干增益和 ENBW 用于幅值、噪声修正。
- goertzel：适合少数频点；目标不超过 Nyquist。非相干采样应加窗。
- cmsis_rfft_q15：256/512 点入口；CMSIS 块缩放由上层恢复。
- spectrum_metrics：THD/THD+N 是 Q15 比值。snr_q8_db、sinad_q8_db、sfdr_q8_db 是有符号 Q8.8 dB。
- transfer_point：同频点 Y/X，实虚部和幅值为 Q12.20。

## NCO、IQ、PLL 与 LMS

- AGC 增益字段沿用 gain_q15 名称但按 Q2.14 解释（0.5 到约 2.0）；AGC 只用于防削顶/显示链，严禁接入幅值、THD、传递函数等保真测量链。
- NCO 使用 32 位相位和二次幂长度正弦表；改频不清相位。
- IQ 解调用一阶低通，输出乘 2 恢复正弦幅值；alpha 太大则二倍频抑制不足。
- PLL 使用 Q30 mHz 积分状态避免逐点截断，并用正向过零 FLL 辅助捕获；相位鉴别器负责细调。掉信号时冻结频率，恢复后重新捕获。
- normalized=0 为 LMS，1 为 NLMS。发散时降低 mu，检查饱和计数。
- 扫频每个驻留点调用 transfer_point；改频时保持相位并丢弃过渡段。

## 验收条件

频率 0.05%、幅值 0.5%、相位 0.5° 是典型目标，不是无条件保证。必须记录采样率、记录长度、幅度、SNR、频偏、窗、模拟前端与时钟误差。低于若干 ADC LSB、斜率不足或削顶时应报告无效。