"""`cpuid` answers as the Xbox's Pentium III (it was a RECOMP_UNIMPL no-op,
so feature tests read whatever the registers held)."""
from .test_port_io import _translate


def test_cpuid_reports_sse_pentium_iii():
    # mov eax, 1; cpuid; ret
    code = _translate(bytes.fromhex('b801000000' '0fa2' 'c3'))
    assert 'RECOMP_UNIMPL' not in code
    assert 'edx = 0x0383F9FFu' in code      # MMX, FXSR, SSE; no SSE2
    assert 'ebx = 0x756E6547u' in code      # "GenuineIntel"
