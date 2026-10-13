"""`fcmovcc` is a conditional st(0) = st(i) on the flags of the last compare
(it used to become a bare comment, so float min/max never moved)."""
from .test_port_io import _translate


def test_fcmovb_after_fcomi_moves_on_below():
    # fld1; fldz; fcomi st(0), st(1); fcmovb st(0), st(1); ret
    code = _translate(bytes.fromhex('d9e8' 'd9ee' 'dbf1' 'dac1' 'c3'))
    assert '/* FPU: fcmov' not in code
    assert 'fp_top() = fp_st(1);' in code
    assert '(g_fp_cmp < 0 || g_fp_cmp == 2)' in code     # jb on fcomi


def test_fcmovnbe_uses_above():
    # fld1; fldz; fcomi st(0), st(1); fcmovnbe st(0), st(1); ret
    code = _translate(bytes.fromhex('d9e8' 'd9ee' 'dbf1' 'dbd1' 'c3'))
    assert '(g_fp_cmp == 1)' in code and 'fp_top() = fp_st(1);' in code


def test_fxtract_splits_exponent_and_significand():
    code = _translate(bytes.fromhex('d9e8' 'd9f4' 'c3'))   # fld1; fxtract; ret
    assert '/* FPU: fxtract' not in code and 'frexp(' in code
