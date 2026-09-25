"""SWF primitives used by the standalone native confirmation resource."""


import struct


class Bits:
    def __init__(self):
        self.values = []

    def put(self, value, count):
        value &= (1 << count) - 1
        self.values.extend((value >> shift) & 1 for shift in range(count - 1, -1, -1))

    def finish(self):
        self.values.extend([0] * (-len(self.values) % 8))
        return bytes(sum(self.values[i + j] << (7 - j) for j in range(8))
                     for i in range(0, len(self.values), 8))


def signed_bits(*values):
    return max(2, *(abs(int(v)).bit_length() + 1 for v in values))


def tag(code, body=b""):
    n = len(body)
    return struct.pack("<H", (code << 6) | min(n, 63)) + (struct.pack("<I", n) if n >= 63 else b"") + body


DEFINITION_TAGS = {2, 6, 7, 10, 11, 14, 20, 21, 22, 32, 33, 34, 35, 36, 37,
                   39, 46, 48, 60, 75, 83, 84, 87, 91}


def tag_start(data):
    return 8 + (5 + 4 * (data[8] >> 3) + 7) // 8 + 4


def read_tags(data, start):
    p = start
    while p < len(data):
        begin = p
        if p + 2 > len(data):
            raise ValueError("Truncated SWF tag header")
        value = struct.unpack_from("<H", data, p)[0]
        p += 2
        kind, size = value >> 6, value & 63
        if size == 63:
            size = struct.unpack_from("<I", data, p)[0]
            p += 4
        if p + size > len(data):
            raise ValueError("Truncated SWF tag body")
        body = data[p:p+size]
        p += size
        yield kind, body, data[begin:p]


def symbol_entries(body):
    count = struct.unpack_from("<H", body)[0]
    p = 2
    entries = []
    for _ in range(count):
        character = struct.unpack_from("<H", body, p)[0]
        p += 2
        end = body.index(0, p)
        entries.append((character, body[p:end].decode("utf-8")))
        p = end + 1
    if p != len(body):
        raise ValueError("Unexpected trailing SymbolClass data")
    return entries


class Reader:
    def __init__(self, data, byte=0):
        self.data, self.bit = data, byte * 8

    def u(self, n):
        result = 0
        for _ in range(n):
            result = result * 2 + ((self.data[self.bit // 8] >> (7 - self.bit % 8)) & 1)
            self.bit += 1
        return result

    def s(self, n):
        v = self.u(n)
        return v - (1 << n) if n and v & (1 << (n - 1)) else v

    def align(self):
        self.bit = (self.bit + 7) // 8 * 8

    def byte(self):
        self.align()
        return self.u(8)

    def word(self):
        return self.byte() | self.byte() << 8


def matrix(r):
    r.align()
    a = d = 1.0
    b = c = 0.0
    if r.u(1):
        n = r.u(5)
        a, d = r.s(n) / 65536, r.s(n) / 65536
    if r.u(1):
        n = r.u(5)
        b, c = r.s(n) / 65536, r.s(n) / 65536
    n = r.u(5)
    x, y = r.s(n), r.s(n)
    r.align()
    return a, b, c, d, x, y


def point(m, p):
    a, b, c, d, x, y = m
    return a*p[0] + c*p[1] + x, b*p[0] + d*p[1] + y
