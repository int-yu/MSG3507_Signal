"""Generate Q15 SOS coefficients plus stability/scaling report."""
import argparse,json
from pathlib import Path
import numpy as np
from scipy import signal
def design(kind,order,cutoff_hz,sample_rate_hz):
    sos=signal.butter(order,cutoff_hz,btype=kind,fs=sample_rate_hz,output="sos")
    poles=np.concatenate([np.roots([1,row[4],row[5]]) for row in sos])
    if np.max(np.abs(poles))>=1: raise ValueError("unstable design")
    cmsis=sos.copy(); cmsis[:,4:]*=-1
    peak=float(np.max(np.abs(cmsis[:,[0,1,2,4,5]])))
    post=max(0,int(np.ceil(np.log2(peak)))) if peak else 0
    while True:
        q=np.rint(cmsis[:,[0,1,2,4,5]]*2**(15-post))
        if np.max(abs(q))<=32767: break
        post+=1
        if post>14: raise ValueError("Q15 overflow")
    return q.astype(np.int16),{"stable":True,
      "max_pole_radius":float(np.max(abs(poles))),"post_shift":post,
      "coefficient_peak":peak,
      "warning":"verify internal peak gain for the intended signal range"}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--kind",choices=["lowpass","highpass"],default="lowpass")
    p.add_argument("--order",type=int,default=4)
    p.add_argument("--cutoff",type=float,required=True)
    p.add_argument("--fs",type=float,required=True)
    p.add_argument("--header",type=Path,required=True)
    p.add_argument("--report",type=Path,required=True); a=p.parse_args()
    q,r=design(a.kind,a.order,a.cutoff,a.fs)
    rows=",\n".join("    {%s, %d}"%(",".join(map(str,row)),r["post_shift"]) for row in q)
    a.header.write_text("#include \"sigq15.h\"\nstatic const sigq15_biquad_coeffs_t generated_sos[] = {\n"+rows+"\n};\n",encoding="utf-8")
    a.report.write_text(json.dumps(r,indent=2),encoding="utf-8")
if __name__=="__main__": main()