"""`fprem` must leave a status that ends the C library's fmod loop
(`fprem; fnstsw ax; sahf; jp again`): C2 clear, whatever compare came before.
It used to touch neither g_fp_cc nor g_fp_cmp, so after an unordered compare
the loop never exited (a FlatOut 2 stunt hung loading)."""
from .test_port_io import _translate


def test_fprem_sets_status_and_compare_state():
    # fld1; fld1; fprem; fnstsw ax; sahf; jp -7 (back to fprem); ret
    code = _translate(bytes.fromhex('d9e8' 'd9e8' 'd9f8' 'dfe0' '9e' '7af9' 'c3'))
    assert 'recomp_fprem(fp_top(), fp_st1(), 0, &_pcc)' in code
    assert 'g_fp_cc = _pcc;' in code
    assert 'g_fp_cmp = RECOMP_FPREM_CMP(_pcc);' in code


def test_fprem1_rounds_to_nearest():
    code = _translate(bytes.fromhex('d9e8' 'd9e8' 'd9f5' 'c3'))   # fprem1
    assert 'recomp_fprem(fp_top(), fp_st1(), 1, &_pcc)' in code
