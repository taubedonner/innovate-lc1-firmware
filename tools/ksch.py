#!/usr/bin/env python3
"""Извлекает список цепей из файла KiCad .kicad_sch (один лист, без шин).

Зачем это здесь. Часть комментариев в src/ ссылается на номиналы и связность
схемы прибора: сопротивление в обратной связи усилителя, номинал резистора в
цепи импульса, положения аналоговых ключей. Все эти факты получены этим
скриптом из схемы, перерисованной в KiCad. Сама схема в репозиторий не
входит - она не наша, - но инструмент выложен, чтобы результат можно было
повторить на своей копии.

Файл .kicad_sch списка цепей НЕ содержит, в нём только геометрия. Связность
восстанавливается так: разобрать s-выражения, собрать выводы из lib_symbols,
пересчитать их координаты для каждого экземпляра с учётом поворота и
зеркала, затем объединить концы проводов, точки соединений, метки и выводы,
попадающие на провод.

Три места, где легко ошибиться:
  * выводы в подсимволе с номером 0 (ИМЯ_0_0) общие для всех секций, а
    экземпляр объявлен как (unit 1) - если брать только запрошенную секцию,
    такие компоненты молча выпадут;
  * одноимённые локальные метки - одна цепь, где бы они ни стояли на листе;
  * силовые символы сливаются по значению (GND, +5V), иначе каждая земля
    окажется отдельной цепью.

Использование: python3 ksch.py schematic.kicad_sch > netlist.txt
"""
import sys, math, re
from collections import defaultdict


def tokenize(s):
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c in '()':
            out.append(c); i += 1
        elif c == '"':
            j = i + 1; buf = []
            while s[j] != '"':
                if s[j] == '\\':
                    buf.append(s[j + 1]); j += 2
                else:
                    buf.append(s[j]); j += 1
            out.append(('str', ''.join(buf))); i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in '()"':
                j += 1
            out.append(('sym', s[i:j])); i = j
    return out


def parse(tokens):
    stack = [[]]
    for t in tokens:
        if t == '(':
            stack.append([])
        elif t == ')':
            node = stack.pop(); stack[-1].append(node)
        else:
            stack[-1].append(t)
    return stack[0][0]


def head(node):
    return node[0][1] if node and isinstance(node[0], tuple) else None


def kids(node, name):
    return [c for c in node if isinstance(c, list) and head(c) == name]


def kid(node, name):
    k = kids(node, name)
    return k[0] if k else None


def val(tok):
    return tok[1] if isinstance(tok, tuple) else tok


def nums(node, start=1):
    return [float(val(x)) for x in node[start:] if isinstance(x, tuple)]


R = 4  # rounding, mm


def rnd(p):
    return (round(p[0], R), round(p[1], R))


# ---------------------------------------------------------------- library

def lib_pins(root):
    """lib_id -> unit -> [(number, name, x, y)] in library coords (Y up)."""
    out = defaultdict(lambda: defaultdict(list))
    ls = kid(root, 'lib_symbols')
    if not ls:
        return out
    for sym in kids(ls, 'symbol'):
        lid = val(sym[1])
        for sub in kids(sym, 'symbol'):
            subname = val(sub[1])
            m = re.match(r'.*_(\d+)_(\d+)$', subname)
            unit = int(m.group(1)) if m else 1
            for pin in kids(sub, 'pin'):
                at = kid(pin, 'at')
                x, y = float(val(at[1])), float(val(at[2]))
                num = val(kid(pin, 'number')[1])
                nm = val(kid(pin, 'name')[1])
                out[lid][unit].append((num, nm, x, y))
    return out


def place(px, py, ox, oy, angle, mx, my):
    """Library coords -> sheet coords."""
    x, y = px, -py                      # library Y is up, sheet Y is down
    if mx:                              # (mirror x): flip vertically
        y = -y
    if my:                              # (mirror y): flip horizontally
        x = -x
    a = math.radians(angle)
    ca, sa = math.cos(a), math.sin(a)
    rx = x * ca + y * sa
    ry = -x * sa + y * ca
    return (ox + rx, oy + ry)


# ---------------------------------------------------------------- geometry

def on_segment(p, a, b, eps=1e-3):
    if min(a[0], b[0]) - eps > p[0] or p[0] > max(a[0], b[0]) + eps:
        return False
    if min(a[1], b[1]) - eps > p[1] or p[1] > max(a[1], b[1]) + eps:
        return False
    cross = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    return abs(cross) < eps


class UF:
    def __init__(self):
        self.p = {}

    def find(self, x):
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]; x = self.p[x]
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[ra] = rb


def main(path):
    root = parse(tokenize(open(path).read()))
    lp = lib_pins(root)

    wires = []
    for w in kids(root, 'wire'):
        pts = kid(w, 'pts')
        xy = [rnd((float(val(p[1])), float(val(p[2])))) for p in kids(pts, 'xy')]
        if len(xy) == 2:
            wires.append(tuple(xy))

    junctions = [rnd((float(val(kid(j, 'at')[1])), float(val(kid(j, 'at')[2]))))
                 for j in kids(root, 'junction')]

    labels = []
    for lab in kids(root, 'label') + kids(root, 'global_label') + kids(root, 'hierarchical_label'):
        at = kid(lab, 'at')
        labels.append((val(lab[1]), rnd((float(val(at[1])), float(val(at[2]))))))

    pins = []       # (ref, value, pin number, pin name, point)
    parts = {}
    for sym in kids(root, 'symbol'):
        lid_n = kid(sym, 'lib_id')
        if not lid_n:
            continue
        lid = val(lid_n[1])
        at = kid(sym, 'at')
        ox, oy = float(val(at[1])), float(val(at[2]))
        angle = float(val(at[3])) if len(at) > 3 else 0.0
        mir = kid(sym, 'mirror')
        mvals = [val(x) for x in mir[1:]] if mir else []
        mx, my = 'x' in mvals, 'y' in mvals
        unit_n = kid(sym, 'unit')
        unit = int(val(unit_n[1])) if unit_n else 1
        ref = value = '?'
        for pr in kids(sym, 'property'):
            k, v = val(pr[1]), val(pr[2])
            if k == 'Reference':
                ref = v
            elif k == 'Value':
                value = v
        parts[ref] = (lid, value)
        # unit 0 holds pins common to every unit of the symbol
        upins = lp.get(lid, {}).get(0, []) + lp.get(lid, {}).get(unit, [])
        for num, nm, px, py in upins:
            pins.append((ref, value, num, nm, rnd(place(px, py, ox, oy, angle, mx, my))))

    # ---- connectivity
    uf = UF()
    for a, b in wires:
        uf.union(a, b)

    endpoints = set()
    for a, b in wires:
        endpoints.add(a); endpoints.add(b)

    # points that must be attached to wires they lie on
    attach = list(junctions) + [p for _, p in labels] + [p for *_, p in pins]
    for p in attach:
        for a, b in wires:
            if p == a or p == b:
                uf.union(p, a)
            elif on_segment(p, a, b):
                uf.union(p, a)   # a junction/pin in the middle of a wire connects

    # a junction also merges wire endpoints coinciding with it
    for j in junctions:
        for a, b in wires:
            if a == j or b == j:
                uf.union(j, a); uf.union(j, b)

    # local labels of the same name are one net, wherever they sit on the sheet
    by_name = defaultdict(list)
    for name, p in labels:
        by_name[name].append(p)
    for name, pts in by_name.items():
        for p in pts[1:]:
            uf.union(pts[0], p)

    # power symbols of the same value are one net as well
    by_power = defaultdict(list)
    for ref, value, num, nm, p in pins:
        if parts[ref][0].startswith('power:'):
            by_power[value].append(p)
    for value, pts in by_power.items():
        for p in pts[1:]:
            uf.union(pts[0], p)

    # ---- naming
    net_names = defaultdict(set)
    for name, p in labels:
        net_names[uf.find(p)].add(name.replace('{slash}', '/'))
    for ref, value, num, nm, p in pins:
        if parts[ref][0].startswith('power:'):
            net_names[uf.find(p)].add(value)

    nets = defaultdict(list)
    for ref, value, num, nm, p in pins:
        if parts[ref][0].startswith('power:'):
            continue
        nets[uf.find(p)].append((ref, value, num, nm))

    out = []
    for root_id, members in nets.items():
        names = sorted(net_names.get(root_id, []))
        label = names[0] if names else 'N$%04d' % (abs(hash(root_id)) % 10000)
        if len(names) > 1:
            label += ' (= ' + ', '.join(names[1:]) + ')'
        out.append((label, sorted(members, key=lambda m: (m[0], m[2]))))
    out.sort(key=lambda n: (n[0].startswith('N$'), n[0]))

    print('# цепей: %d, компонентов: %d, выводов: %d' % (len(out), len(parts), len(pins)))
    for label, members in out:
        print('\n%s' % label)
        for ref, value, num, nm in members:
            print('    %-6s %-18s вывод %-4s %s' % (ref, value, num, nm))


if __name__ == '__main__':
    main(sys.argv[1])
