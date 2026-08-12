"""NumPy/SciPy golden references; never linked into the MCU real-time path."""
from __future__ import annotations
import numpy as np
from scipy import signal
def q15(values):
    return np.clip(np.rint(np.asarray(values)*32768.0),-32768,32767).astype(np.int16)
def tone(fs, frequency, count, amplitude=.5, phase_turns=0.0):
    n=np.arange(count)
    return amplitude*np.sin(2*np.pi*(frequency*n/fs+phase_turns))
def rms(values):
    x=np.asarray(values,dtype=np.float64)
    return float(np.sqrt(np.mean(x*x)))
def goertzel(values,fs,frequency):
    x=np.asarray(values,dtype=np.float64); n=np.arange(x.size)
    z=np.sum(x*np.exp(-2j*np.pi*frequency*n/fs))
    return 2*abs(z)/x.size, np.angle(z)/(2*np.pi)%1.0
def window(name,count):
    names={"hann":"hann","blackman_harris":"blackmanharris",
           "flat_top":"flattop","rect":"boxcar"}
    return signal.get_window(names[name],count,fftbins=False)
def spectrum_metrics(values,fs,fundamental,harmonics=5):
    x=np.asarray(values,dtype=np.float64); power=abs(np.fft.rfft(x))**2
    k=int(round(fundamental*x.size/fs))
    hs=[k*h for h in range(2,harmonics+1) if k*h<power.size]
    hp=sum(power[h] for h in hs); excluded={0,k,*hs}
    noise=sum(v for i,v in enumerate(power) if i not in excluded)
    return {"thd":np.sqrt(hp/power[k]),"thdn":np.sqrt((hp+noise)/power[k])}