"""A je/jne after a join whose predecessors set ZF with different
instructions on the same register tests the result snapshot (_fa), not the
register read back at the branch, which may have been replaced in between.
Shape of FlatOut 2's bit reader 0x1C63F0: and/jns/dec/or/inc into one join,
then mov+lea over ecx, then jne."""
from .test_port_io import _translate

CODE = bytes.fromhex(
    '81e107000080'   # and ecx, 0x80000007
    '7905'           # jns +5 (to the mov)
    '49'             # dec ecx
    '83c9f8'         # or ecx, -8
    '41'             # inc ecx
    '8b4b08'         # mov ecx, [ebx+8]
    '8d0c01'         # lea ecx, [ecx+eax]
    '7501'           # jne +1
    '90'             # nop
    'c3')            # ret


def test_jne_after_merged_zf_reads_the_snapshot():
    code = _translate(CODE)
    jne = [l for l in code.splitlines() if 'goto' in l and 'jne' in l]
    assert jne, code
    assert '(_fa != 0)' in jne[0], jne[0]
    assert '(ecx != 0)' not in jne[0]
