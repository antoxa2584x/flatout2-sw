"""`in`/`out` lift to the runtime's port accessors (they were RECOMP_UNIMPL
no-ops, so `in` left the accumulator unchanged)."""
from . import config
from .translator import FunctionTranslator


def _translate(image):
    base = 0x10000
    config._install([config.Section('.text', base, len(image), 0, len(image), True)],
                    entry_point=base, kernel_thunk_addr=base, origin='port_io')
    db = {base: {'start': hex(base), 'end': base + len(image), '_addr': base, 'size': len(image)}}
    return FunctionTranslator(image, db).translate_function(base, db[base])


def test_in_al_dx_reads_the_port():
    # mov dx, 0x80c0; in al, dx; ret
    code = _translate(bytes.fromhex('66bac080' 'ec' 'c3'))
    assert 'RECOMP_UNIMPL' not in code
    assert 'xbox_PortIn(LO16(edx), 1)' in code


def test_in_eax_imm_reads_the_port():
    code = _translate(bytes.fromhex('e5c0' 'c3'))           # in eax, 0xc0; ret
    assert 'xbox_PortIn(0x00C0u, 4)' in code


def test_out_dx_al_writes_the_port():
    code = _translate(bytes.fromhex('ee' 'c3'))             # out dx, al; ret
    assert 'xbox_PortOut(LO16(edx),' in code and ', 1);' in code
