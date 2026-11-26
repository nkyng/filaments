"""Junta os STL da caixa num unico 3MF (um objeto por peca, ja separados lado a lado).

Uso: python3 gerar_3mf.py   (gera stl/central-nfc-v0.3mf)
"""
import struct
import zipfile
from pathlib import Path

HERE = Path(__file__).parent
STL = HERE / "stl"
OUT = STL / "central-nfc-v0.3mf"
GAP = 15.0

# ordem = ordem no fatiador
PARTS = ["bandeja", "tampa", "prato", "pod", "cone"]


def read_stl(path):
    data = path.read_bytes()
    if data[:5] == b"solid" and b"facet" in data[:300]:
        tris, cur = [], []
        for line in data.decode().splitlines():
            line = line.strip()
            if line.startswith("vertex"):
                cur.append(tuple(float(v) for v in line.split()[1:4]))
                if len(cur) == 3:
                    tris.append(cur)
                    cur = []
        return tris
    n = struct.unpack("<I", data[80:84])[0]
    tris = []
    for i in range(n):
        off = 84 + i * 50
        v = struct.unpack("<12f", data[off:off + 48])
        tris.append([v[3:6], v[6:9], v[9:12]])
    return tris


def mesh_xml(tris):
    index, verts, faces = {}, [], []
    for t in tris:
        ids = []
        for p in t:
            key = tuple(round(c, 5) for c in p)
            if key not in index:
                index[key] = len(verts)
                verts.append(key)
            ids.append(index[key])
        if len(set(ids)) == 3:
            faces.append(ids)
    mn = [min(v[i] for v in verts) for i in range(3)]
    mx = [max(v[i] for v in verts) for i in range(3)]
    vx = "".join(f'<vertex x="{x:.5f}" y="{y:.5f}" z="{z:.5f}"/>' for x, y, z in verts)
    fx = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in faces)
    return f"<mesh><vertices>{vx}</vertices><triangles>{fx}</triangles></mesh>", mn, mx


def main():
    objects, items = [], []
    x_cursor = 0.0
    for i, name in enumerate(PARTS, start=1):
        xml, mn, mx = mesh_xml(read_stl(STL / f"{name}.stl"))
        objects.append(f'<object id="{i}" name="{name}" type="model">{xml}</object>')
        # coloca a peca com a base em z=0, lado a lado no eixo X, centradas em Y
        w = mx[0] - mn[0]
        tx = x_cursor - mn[0]
        ty = -(mn[1] + mx[1]) / 2
        tz = -mn[2]
        items.append(f'<item objectid="{i}" transform="1 0 0 0 1 0 0 0 1 {tx:.4f} {ty:.4f} {tz:.4f}"/>')
        x_cursor += w + GAP

    model = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<model unit="millimeter" xml:lang="pt-BR" '
        'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
        '<metadata name="Title">Central NFC de Filamentos v0</metadata>'
        f'<resources>{"".join(objects)}</resources>'
        f'<build>{"".join(items)}</build></model>'
    )
    content_types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
        "</Types>"
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
        "</Relationships>"
    )
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
    print(f"{OUT} ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
