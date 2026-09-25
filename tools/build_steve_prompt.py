"""Add a dormant native button to ConfirmationBox in the local ui.swf.

Ordinary prompts keep it on an empty, stopped frame. Only the matching
save-scum exit prompt activates it. Original clips, labels and scripts remain.
"""
from pathlib import Path
import argparse
import struct
import zlib
from abc_patch import append_button_class
from game_archive import GameArchive
from swf import Bits, signed_bits, tag, read_tags, tag_start, symbol_entries, DEFINITION_TAGS
from swf import Reader, matrix

ROOT = Path(__file__).resolve().parents[1]
NODE = 'fos_steve_exit'


def read_base():
    archive = GameArchive(ROOT.parents[1] / 'resources.gpak')
    offset, size = archive.entries['swfs/ui.swf']
    with archive.path.open('rb') as stream:
        stream.seek(archive.base + offset)
        data = stream.read(size)
    if data[:3] == b'CWS':
        data = b'FWS' + data[3:8] + zlib.decompress(data[8:])
    if data[:3] != b'FWS' or len(data) != struct.unpack_from('<I', data, 4)[0]:
        raise ValueError('Unsupported ui.swf')
    return data


def placement(body):
    reader = Reader(body)
    flags, depth = reader.byte(), reader.word()
    child = reader.word() if flags & 2 else None
    transform = matrix(reader) if flags & 4 else None
    color = None
    if flags & 8:
        start = reader.bit // 8
        add, multiply, bits = reader.u(1), reader.u(1), reader.u(4)
        reader.u(4 * bits * (add + multiply)); reader.align()
        color = body[start:reader.bit // 8]
    if flags & 16:
        reader.word()
    name = body[reader.bit // 8:].split(b'\0')[0].decode('ascii') if flags & 32 else ''
    return flags, depth, child, transform, color, name


def encode_matrix(transform):
    a, b, c, d, x, y = transform
    out = Bits()
    out.put(1, 1)
    scales = [round(v * 65536) for v in (a, d)]
    n = signed_bits(*scales); out.put(n, 5)
    for value in scales: out.put(value, n)
    out.put(bool(b or c), 1)
    if b or c:
        skews = [round(v * 65536) for v in (b, c)]
        n = signed_bits(*skews); out.put(n, 5)
        for value in skews: out.put(value, n)
    x, y = round(x), round(y)
    n = signed_bits(x, y); out.put(n, 5); out.put(x, n); out.put(y, n)
    return out.finish()


def hidden_button(definitions, original, ident):
    kind, body = definitions[original]
    if kind != 39:
        raise ValueError('Confirmation button is not a MovieClip')
    states = {}; current = None
    for code, data, raw in read_tags(body, 4):
        if code == 43:
            current = data.split(b'\0')[0].decode('ascii'); states[current] = []
        elif code == 26 and current:
            flags, depth, child, *_ = placement(data)
            if child is None:
                raise ValueError('Button artwork expected static placements')
            states[current].append((depth, tag(26, bytes([flags & ~1]) + data[1:])))
    names = ['hidden', 'up', 'over', 'down', 'selected', 'disabled', 'enable']
    clips = [[], states['up'], states['over'], states['down'], states['down'], states['disabled'], states['up']]
    frames = bytearray(); prior = []
    for name, children in zip(names, clips):
        frames += tag(43, name.encode() + b'\0')
        for depth in prior: frames += tag(28, struct.pack('<H', depth))
        for depth, raw in children: frames += raw
        prior = [depth for depth, _ in children]
        frames += tag(1)
    return struct.pack('<HH', ident, len(names)) + frames + tag(0)


def extend_prompt(body, ident):
    entries = list(read_tags(body, 4))
    named = {placement(data)[5]: placement(data) for code, data, _ in entries if code == 26 and data[0] & 32}
    yes, no = named['yes'], named['no']
    if yes[2] != no[2] or NODE in named:
        raise ValueError('Unexpected confirmation button layout')
    depth = max(placement(data)[1] for code, data, _ in entries if code == 26) + 1
    transforms = {yes[1]: yes[3], no[1]: no[3]}
    alive = set(); color = None; added = False; output = bytearray(body[:4])
    for code, data, raw in entries:
        if code == 26:
            flags, at, child, transform, tint, _ = placement(data)
            if at in transforms:
                if child is not None: alive.add(at)
                if transform: transforms[at] = transform
                if at == yes[1] and tint is not None: color = tint
        elif code == 28:
            alive.discard(struct.unpack_from('<H', data)[0])
        if code == 1:
            if yes[1] in alive and no[1] in alive:
                left, right = transforms[yes[1]], transforms[no[1]]
                # Same paper button, centered in a third row below Yes/No.
                transform = (left[0], left[1], left[2], left[3], (left[4]+right[4])/2, left[5]+2800)
                flags = (0x26 if not added else 0x05) | (8 if color is not None else 0)
                extra = bytes([flags]) + struct.pack('<H', depth)
                if not added: extra += struct.pack('<H', ident)
                extra += encode_matrix(transform)
                if color is not None: extra += color
                if not added: extra += NODE.encode() + b'\0'
                output += tag(26, extra); added = True
            elif added:
                output += tag(28, struct.pack('<H', depth)); added = False
        output += raw
    return bytes(output), yes[2], depth


def build(base=None):
    base = read_base() if base is None else base
    entries = list(read_tags(base, tag_start(base)))
    definitions = {struct.unpack_from('<H', b)[0]: (k, b) for k, b, _ in entries if k in DEFINITION_TAGS}
    symbols = [(b, symbol_entries(b)) for k, b, _ in entries if k == 76]
    scripts = [b for k, b, _ in entries if k == 82]
    if len(symbols) != 1 or len(scripts) != 1:
        raise ValueError('Expected one SymbolClass and one DoABC block')
    prompt = next(i for i, name in symbols[0][1] if name == 'ConfirmationBox')
    ident = max(definitions) + 1
    if ident > 65535: raise ValueError('No character IDs remain')
    prompt_body, original_button, depth = extend_prompt(definitions[prompt][1], ident)
    button = hidden_button(definitions, original_button, ident)
    script = scripts[0]; offset = script.index(0, 4) + 1
    abc, class_name = append_button_class(script[offset:], b'FuckOffSteveExitButton', 7)
    output = bytearray(base[:tag_start(base)])
    inserted = False
    for kind, body, raw in entries:
        if kind == 39 and struct.unpack_from('<H', body)[0] == prompt:
            output += tag(39, button) + tag(39, prompt_body); inserted = True
        elif kind == 82:
            output += tag(82, script[:offset] + abc)
        elif kind == 76:
            count = struct.unpack_from('<H', body)[0]
            output += tag(76, struct.pack('<H', count+1) + body[2:] + struct.pack('<H', ident) + class_name.encode() + b'\0')
        else: output += raw
    if not inserted: raise ValueError('ConfirmationBox not found')
    struct.pack_into('<I', output, 4, len(output))
    return bytes(output), {'prompt': prompt, 'button': ident, 'depth': depth, 'class': class_name}


def write_game_asset(data, target):
    # Mewgenics' reader at 0xA54340 reads RECT/tags directly after byte 8.
    # It does not decompress CWS; a standard-compliant compressed SWF can
    # therefore crash its loader. Validate the actual deployment bytes.
    if data[:3] != b'FWS' or len(data) < 8 or struct.unpack_from('<I', data, 4)[0] != len(data):
        raise ValueError('Mewgenics requires an uncompressed FWS with an exact file length')
    list(read_tags(data, tag_start(data)))
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build' / 'ui.swf')
    args = parser.parse_args()
    data, info = build()
    write_game_asset(data, args.output)
    print(f'Steven exit button staged in {args.output}: {info}; uncompressed FWS, one DoABC, original prompt timeline preserved.')
