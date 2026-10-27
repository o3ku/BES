import struct, sys, re, io

def dec(b):
    try:
        return b.decode('gbk')
    except Exception:
        return b.decode('latin1')

class R:
    def __init__(self, buf, pos):
        self.b = buf; self.p = pos
    def u8(self):
        v = self.b[self.p]; self.p += 1; return v
    def u16(self):
        v = struct.unpack_from('<H', self.b, self.p)[0]; self.p += 2; return v
    def u32(self):
        v = struct.unpack_from('<I', self.b, self.p)[0]; self.p += 4; return v
    def i64(self):
        v = struct.unpack_from('<q', self.b, self.p)[0]; self.p += 8; return v
    def sstr(self):
        n = self.u8(); s = self.b[self.p:self.p+n]; self.p += n; return dec(s)
    def peek(self):
        return self.b[self.p]

def read_value(r, t, name, out, indent):
    pre = '  '*indent + name + ' = '
    if t == 0:
        out.append(pre + '<null>')
    elif t == 1:
        # vaList: list of shortstrings (TStrings), terminated by 0x00
        items = []
        while True:
            if r.peek() == 0:
                r.u8(); break
            items.append(r.sstr())
        out.append(pre + '[' + ', '.join('"%s"' % x for x in items) + ']')
    elif t == 2:
        out.append(pre + str(r.u8()))
    elif t == 3:
        out.append(pre + str(r.u16()))
    elif t == 4:
        out.append(pre + str(r.u32()))
    elif t == 5:
        r.p += 10
        out.append(pre + '<extended>')
    elif t in (6, 7):
        out.append(pre + '"%s"' % r.sstr())
    elif t == 8:
        out.append(pre + 'False')
    elif t == 9:
        out.append(pre + 'True')
    elif t == 10:
        n = r.u32(); r.p += n
        out.append(pre + '<binary %d bytes>' % n)
    elif t == 11:
        items = []
        while True:
            n = r.u8()
            if n == 0: break
            items.append(r.b[r.p:r.p+n].decode('latin1')); r.p += n
        out.append(pre + '[' + ', '.join(items) + ']')
    elif t == 12:
        n = r.u32(); s = r.b[r.p:r.p+n]; r.p += n
        out.append(pre + '"%s"' % dec(s))
    elif t == 13:
        out.append(pre + 'nil')
    elif t == 14:
        out.append(pre + '<collection>')
        while True:
            if r.peek() == 0:
                r.u8(); break
            r.u8()   # item marker
            read_props(r, out, indent+1)
    elif t == 15:
        v = struct.unpack_from('<f', r.b, r.p)[0]; r.p += 4
        out.append(pre + repr(v))
    elif t == 16:
        r.p += 8; out.append(pre + '<currency>')
    elif t == 17:
        r.p += 8; out.append(pre + '<date>')
    elif t == 18:
        n = r.u32(); s = r.b[r.p:r.p+n]; r.p += n
        out.append(pre + '"%s"' % s.decode('utf-16-le', 'replace'))
    elif t == 19:
        out.append(pre + str(r.i64()))
    elif t == 20:
        n = r.u32(); s = r.b[r.p:r.p+n]; r.p += n
        out.append(pre + '"%s"' % s.decode('utf-8', 'replace'))
    else:
        raise ValueError('bad value type %d for %s' % (t, name))

def read_props(r, out, indent):
    while True:
        nlen = r.u8()
        if nlen == 0:
            return
        nstart = r.p
        name = r.b[r.p:r.p+nlen].decode('latin1'); r.p += nlen
        t = r.u8()
        try:
            read_value(r, t, name, out, indent)
        except ValueError:
            out.append('  '*indent + '!! BADPROP off=0x%X name=%r' % (nstart, r.b[nstart-40:nstart+40]))
            raise

def read_obj(r, out, indent):
    cls = r.sstr()
    name = r.sstr()
    out.append('  '*indent + 'object %s: %s' % (name, cls))
    read_props(r, out, indent+1)
    while True:
        if r.peek() == 0:
            r.u8(); break
        read_obj(r, out, indent+1)

def main(fn, outfn):
    d = open(fn, 'rb').read()
    allout = []
    for m in re.finditer(b'TPF0', d):
        off = m.start()
        r = R(d, off+4)
        out = []
        try:
            read_obj(r, out, 0)
        except Exception as e:
            out.append('  ERROR %r (stopped at +0x%X, abs 0x%X)' % (e, r.p-off, r.p))
            out.append('  CONTEXT: %r' % d[r.p-60:r.p+60])
        allout.append('\n\n======== TPF0 @ 0x%X ========' % off)
        allout.extend(out)
    open(outfn, 'w', encoding='utf-8').write('\n'.join(allout))
    print('done', outfn)

main(sys.argv[1], sys.argv[2])
