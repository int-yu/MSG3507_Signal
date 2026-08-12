from pathlib import Path
import re,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).parents[1]/"tools"))
from golden_model import goertzel,q15,rms,tone,window
from design_iir import design
ROOT=Path(__file__).parents[1]
def test_golden_tone_measurements():
    x=tone(1600,100,256,.6); amplitude,_=goertzel(x,1600,100)
    assert abs(rms(x)-.6/np.sqrt(2))<1e-12
    assert abs(amplitude-.6)<1e-12
    assert q15([-.5,0,.5]).tolist()==[-16384,0,16384]
def test_window_invariants():
    for name in ("hann","blackman_harris","flat_top"):
        w=window(name,257)
        assert len(w)==257 and np.all(np.isfinite(w))
    assert window("hann",257)[0]==0
def test_sos_quantizer_stability_and_range():
    coeffs,report=design("lowpass",4,1000,20000)
    assert report["stable"] and report["max_pole_radius"]<1
    assert coeffs.dtype==np.int16 and abs(coeffs.astype(np.int32)).max()<=32767
def test_target_source_has_no_float_or_allocation():
    source="\n".join(p.read_text(encoding="utf-8") for p in (ROOT/"src").glob("*.c"))
    assert not re.search(r"\b(float|double|malloc|calloc|realloc|free)\b",source)
    assert "math.h" not in source