"""shl/shr by cl mask the count to 5 bits, as x86 does: an unmasked C shift
of 32 or more is undefined (FlatOut 2's bitstream decoder built
(1 << bits) - 1 from a 7-bit field)."""
from .test_port_io import _translate


def test_shl_by_cl_masks_count():
    code = _translate(bytes.fromhex('d3e0' 'c3'))          # shl eax, cl; ret
    assert '((LO8(ecx)) & 31u)' in code


def test_shr_by_cl_masks_count():
    code = _translate(bytes.fromhex('d3e8' 'c3'))          # shr eax, cl; ret
    assert '((LO8(ecx)) & 31u)' in code


def test_shl_by_constant_unchanged():
    code = _translate(bytes.fromhex('c1e004' 'c3'))        # shl eax, 4; ret
    assert '& 31u)' not in code
