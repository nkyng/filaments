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

# cores (3MF basematerials): o Bambu Studio/Orca mostram um dialogo para ligar
# cada cor a um filamento do AMS
COLORS = [("Preto", "#1A1A1AFF"), ("Cinza", "#8E9193FF")]
PRETO, CINZA = 0, 1
PART_COLOR = {"bandeja": CINZA, "tampa": PRETO, "prato": CINZA, "pod": PRETO, "cone": PRETO}
MAT_ID = len(PARTS) + 1


def prato_mark(tri):
    """Fundo da gravacao NFC no prato (z = 0.4 na posicao de impressao) -> preto."""
    return all(abs(v[2] - 0.4) < 1e-3 for v in tri)


TRIANGLE_COLOR = {"prato": (prato_mark, PRETO)}


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


def mesh_xml(tris, tri_color=None):
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
            color = None
            if tri_color and tri_color[0](t):
                color = tri_color[1]
            faces.append((ids, color))
    mn = [min(v[i] for v in verts) for i in range(3)]
    mx = [max(v[i] for v in verts) for i in range(3)]
    vx = "".join(f'<vertex x="{x:.5f}" y="{y:.5f}" z="{z:.5f}"/>' for x, y, z in verts)
    fx = "".join(
        f'<triangle v1="{a}" v2="{b}" v3="{c}"' + (f' p1="{col}"' if col is not None else "") + "/>"
        for (a, b, c), col in faces
    )
    return f"<mesh><vertices>{vx}</vertices><triangles>{fx}</triangles></mesh>", mn, mx


def main():
    objects, items = [], []
    x_cursor = 0.0
    for i, name in enumerate(PARTS, start=1):
        xml, mn, mx = mesh_xml(read_stl(STL / f"{name}.stl"), TRIANGLE_COLOR.get(name))
        objects.append(
            f'<object id="{i}" name="{name}" type="model" pid="{MAT_ID}" pindex="{PART_COLOR[name]}">{xml}</object>'
        )
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
        f'<resources><basematerials id="{MAT_ID}">'
        + "".join(f'<base name="{n}" displaycolor="{c}"/>' for n, c in COLORS)
        + f'</basematerials>{"".join(objects)}</resources>'
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
