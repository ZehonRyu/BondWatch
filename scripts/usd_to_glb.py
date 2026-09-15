"""Convert a USDZ character (mixamo + UsdPreviewSurface) into a skinned GLB."""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade, UsdSkel

JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942


def pad4(data: bytes) -> bytes:
    extra = (4 - (len(data) % 4)) % 4
    return data + (b" " if extra and False else b"\x00") * extra


def pad_json(data: bytes) -> bytes:
    extra = (4 - (len(data) % 4)) % 4
    return data + b" " * extra


def mat_gltf(m: Gf.Matrix4d) -> list[float]:
    """USD row-vector affine -> glTF column-vector, column-major floats."""
    out: list[float] = []
    for r in range(4):
        for c in range(4):
            out.append(float(m[r][c]))
    return out


def mat_inverse(m: Gf.Matrix4d) -> Gf.Matrix4d:
    try:
        return m.GetInverse()
    except Exception:
        return Gf.Matrix4d(1.0)


def trs_from_matrix(m: Gf.Matrix4d) -> dict:
    xf = Gf.Transform(m)
    t = xf.GetTranslation()
    q = xf.GetRotation().GetQuaternion()
    i = q.GetImaginary()
    s = xf.GetScale()
    return {
        "translation": [float(t[0]), float(t[1]), float(t[2])],
        "rotation": [float(i[0]), float(i[1]), float(i[2]), float(q.GetReal())],
        "scale": [float(s[0]), float(s[1]), float(s[2])],
    }


def asset_str(val) -> str:
    if val is None:
        return ""
    if isinstance(val, Sdf.AssetPath):
        raw = val.path or val.resolvedPath or ""
        return raw.strip("@")
    return str(val).strip().strip("@")


def triangulate(counts, indices: list[int]) -> list[int]:
    tris: list[int] = []
    cursor = 0
    for count in counts:
        count = int(count)
        face = indices[cursor : cursor + count]
        cursor += count
        for i in range(1, count - 1):
            tris.extend((face[0], face[i], face[i + 1]))
    return tris


class Bin:
    def __init__(self) -> None:
        self.buf = bytearray()
        self.views: list[dict] = []
        self.accessors: list[dict] = []

    def add(self, data: bytes, target: int | None = None) -> int:
        extra = (4 - (len(self.buf) % 4)) % 4
        self.buf.extend(b"\x00" * extra)
        view = {"buffer": 0, "byteOffset": len(self.buf), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.views.append(view)
        self.buf.extend(data)
        return len(self.views) - 1

    def accessor(self, data: bytes, comp: str, count: int, typ: str, target: int | None, mn=None, mx=None) -> int:
        view = self.add(data, target)
        acc: dict = {"bufferView": view, "componentType": {"f": 5126, "H": 5123, "B": 5121}[comp], "count": count, "type": typ}
        if mn is not None:
            acc["min"] = mn
            acc["max"] = mx
        self.accessors.append(acc)
        return len(self.accessors) - 1


def pack_f32(values) -> bytes:
    return struct.pack("<" + "f" * len(values), *values)


def pack_u16(values) -> bytes:
    return struct.pack("<" + "H" * len(values), *values)


def shader_input(shader: UsdShade.Shader, name: str):
    inp = shader.GetInput(name)
    if not inp:
        return None, []
    return inp.Get(), inp.GetConnectedSources()[0]


def material_info(stage: Usd.Stage, mat: UsdShade.Material, search_dir: Path) -> dict:
    color = (0.75, 0.75, 0.75)
    tex = None
    surface = mat.GetSurfaceOutput()
    cons = surface.GetConnectedSources()[0] if surface else []
    shader = UsdShade.Shader(cons[0].source.GetPrim()) if cons else UsdShade.Shader()
    val, linked = shader_input(shader, "diffuseColor") if shader else (None, [])
    if val is not None:
        color = (float(val[0]), float(val[1]), float(val[2]))
    if linked:
        tex_shader = UsdShade.Shader(linked[0].source.GetPrim())
        file_val, _ = shader_input(tex_shader, "file")
        rel = asset_str(file_val)
        if rel:
            cand = (search_dir / rel).resolve()
            if cand.is_file():
                tex = cand
    rough_val, _ = shader_input(shader, "roughness") if shader else (None, [])
    return {"color": color, "tex": tex, "rough": float(rough_val) if rough_val is not None else 0.5}


def texture_root(usdz: Path) -> Path:
    if (usdz.parent / "textures").is_dir():
        return usdz.parent
    import tempfile
    import zipfile

    tmp = Path(tempfile.mkdtemp(prefix="usdz_"))
    with zipfile.ZipFile(usdz) as zf:
        zf.extractall(tmp)
    return tmp


def convert(usdz: Path, out_glb: Path) -> None:
    stage = Usd.Stage.Open(str(usdz))
    if not stage:
        raise SystemExit(f"cannot open {usdz}")
    search_dir = texture_root(usdz)

    skel_prim = None
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Skeleton):
            skel_prim = prim
            break
    if not skel_prim:
        raise SystemExit("no skeleton")
    skeleton = UsdSkel.Skeleton(skel_prim)
    cache = UsdSkel.Cache()
    skel_query = cache.GetSkelQuery(skeleton)
    joint_paths = list(skel_query.GetJointOrder())
    bind_xfs = list(skel_query.GetJointWorldBindTransforms())
    joint_names = [p.split("/")[-1] for p in joint_paths]
    parent_of = []
    index_of = {p: i for i, p in enumerate(joint_paths)}
    for path in joint_paths:
        if "/" not in path:
            parent_of.append(-1)
        else:
            parent_of.append(index_of[path.rsplit("/", 1)[0]])
    rest_xfs = []
    for i, world in enumerate(bind_xfs):
        if parent_of[i] < 0:
            rest_xfs.append(world)
        else:
            rest_xfs.append(world * mat_inverse(bind_xfs[parent_of[i]]))

    binary = Bin()
    nodes: list[dict] = []
    meshes: list[dict] = []
    materials: list[dict] = []
    textures: list[dict] = []
    images: list[dict] = []
    samplers = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]
    image_index: dict[str, int] = {}

    root = {"name": "lumi", "children": []}
    nodes.append(root)
    armature = {"name": "Armature", "children": []}
    nodes.append(armature)
    root["children"].append(1)

    joint_node = []
    for i, name in enumerate(joint_names):
        src = rest_xfs[i] if i < len(rest_xfs) else Gf.Matrix4d(1.0)
        node = {"name": name, "children": []}
        node.update(trs_from_matrix(src))
        joint_node.append(len(nodes))
        nodes.append(node)
    for i, p in enumerate(parent_of):
        idx = joint_node[i]
        if p < 0:
            armature["children"].append(idx)
        else:
            nodes[joint_node[p]]["children"].append(idx)
    for node in nodes:
        if "children" in node and not node["children"]:
            del node["children"]

    ibm: list[float] = []
    for xf in bind_xfs:
        ibm.extend(mat_gltf(mat_inverse(xf)))
    ibm_acc = binary.accessor(pack_f32(ibm), "f", len(joint_paths), "MAT4", None)
    skin = {
        "name": "mixamo",
        "skeleton": joint_node[0],
        "joints": joint_node,
        "inverseBindMatrices": ibm_acc,
    }

    def ensure_image(path: Path) -> int:
        key = str(path)
        if key in image_index:
            return image_index[key]
        data = path.read_bytes()
        view = binary.add(data)
        mime = "image/jpeg" if path.suffix.lower() in {".jpg", ".jpeg"} else "image/png"
        images.append({"bufferView": view, "mimeType": mime})
        textures.append({"sampler": 0, "source": len(images) - 1})
        image_index[key] = len(textures) - 1
        return image_index[key]

    mat_cache: dict[str, int] = {}

    def ensure_material(mat: UsdShade.Material) -> int:
        key = str(mat.GetPath())
        if key in mat_cache:
            return mat_cache[key]
        info = material_info(stage, mat, search_dir)
        gmat = {
            "name": mat.GetPrim().GetName(),
            "pbrMetallicRoughness": {
                "baseColorFactor": [info["color"][0], info["color"][1], info["color"][2], 1.0],
                "metallicFactor": 0.0,
                "roughnessFactor": info["rough"],
            },
        }
        if info["tex"] is not None:
            tex_i = ensure_image(info["tex"])
            gmat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": tex_i}
        mat_cache[key] = len(materials)
        materials.append(gmat)
        return mat_cache[key]

    for prim in stage.Traverse():
        if not prim.IsA(UsdGeom.Mesh):
            continue
        mesh = UsdGeom.Mesh(prim)
        pts = list(mesh.GetPointsAttr().Get() or [])
        counts = list(mesh.GetFaceVertexCountsAttr().Get() or [])
        fvi = [int(v) for v in (mesh.GetFaceVertexIndicesAttr().Get() or [])]
        if not pts or not fvi:
            continue
        tris = triangulate(counts, fvi)
        pos = []
        mn = [1e9, 1e9, 1e9]
        mx = [-1e9, -1e9, -1e9]
        for p in pts:
            xyz = (float(p[0]), float(p[1]), float(p[2]))
            pos.extend(xyz)
            for i, v in enumerate(xyz):
                mn[i] = min(mn[i], v)
                mx[i] = max(mx[i], v)
        pos_acc = binary.accessor(pack_f32(pos), "f", len(pts), "VEC3", 34962, mn, mx)
        idx_acc = binary.accessor(pack_u16(tris), "H", len(tris), "SCALAR", 34963)

        attributes = {"POSITION": pos_acc}
        st = UsdGeom.PrimvarsAPI(prim).GetPrimvar("st")
        if st:
            raw_uv = list(st.Get() or [])
            uvs = []
            for i in range(len(pts)):
                uv = raw_uv[i] if i < len(raw_uv) else (raw_uv[-1] if raw_uv else (0.0, 0.0))
                uvs.extend((float(uv[0]), 1.0 - float(uv[1])))
            attributes["TEXCOORD_0"] = binary.accessor(pack_f32(uvs), "f", len(pts), "VEC2", 34962)

        ji = UsdGeom.PrimvarsAPI(prim).GetPrimvar("skel:jointIndices")
        jw = UsdGeom.PrimvarsAPI(prim).GetPrimvar("skel:jointWeights")
        if ji and jw:
            elem = int(ji.GetElementSize() or 1)
            idx_vals = [int(v) for v in (ji.Get() or [])]
            w_vals = [float(v) for v in (jw.Get() or [])]
            joints = []
            weights = []
            n_joints = len(joint_paths)
            for v in range(len(pts)):
                base = v * elem
                pairs = []
                for k in range(elem):
                    if base + k >= len(idx_vals):
                        break
                    j = idx_vals[base + k]
                    w = w_vals[base + k] if base + k < len(w_vals) else 0.0
                    if 0 <= j < n_joints and w > 0:
                        pairs.append((j, w))
                pairs.sort(key=lambda x: x[1], reverse=True)
                pairs = pairs[:4]
                while len(pairs) < 4:
                    pairs.append((0, 0.0))
                js = [p[0] for p in pairs]
                ws = [p[1] for p in pairs]
                total = sum(ws) or 1.0
                ws = [w / total for w in ws]
                joints.extend(js)
                weights.extend(ws)
            attributes["JOINTS_0"] = binary.accessor(pack_u16(joints), "H", len(pts), "VEC4", 34962)
            attributes["WEIGHTS_0"] = binary.accessor(pack_f32(weights), "f", len(pts), "VEC4", 34962)

        mat_i = 0
        bound, _ = UsdShade.MaterialBindingAPI(prim).ComputeBoundMaterial()
        if bound:
            mat_i = ensure_material(bound)

        mesh_i = len(meshes)
        meshes.append({"name": prim.GetParent().GetName(), "primitives": [{"attributes": attributes, "indices": idx_acc, "material": mat_i}]})
        mesh_node = {"name": prim.GetParent().GetName(), "mesh": mesh_i}
        if "WEIGHTS_0" in attributes:
            mesh_node["skin"] = 0
        nodes.append(mesh_node)
        root.setdefault("children", []).append(len(nodes) - 1)

    animations = []
    anim_prim = None
    for prim in stage.Traverse():
        if prim.IsA(UsdSkel.Animation):
            anim_prim = prim
            break
    if anim_prim:
        usd_anim = UsdSkel.Animation(anim_prim)
        anim_joints = [str(j) for j in (usd_anim.GetJointsAttr().Get() or [])]
        joint_index = {str(p): i for i, p in enumerate(joint_paths)}
        rot_attr = usd_anim.GetRotationsAttr()
        trans_attr = usd_anim.GetTranslationsAttr()
        scale_attr = usd_anim.GetScalesAttr()
        samples = sorted(set(rot_attr.GetTimeSamples() or []) | set(trans_attr.GetTimeSamples() or []))
        fps = float(stage.GetTimeCodesPerSecond() or 30.0)
        if samples:
            t0 = samples[0]
            times = [(float(t) - float(t0)) / fps for t in samples]
            time_acc = binary.accessor(
                pack_f32(times), "f", len(times), "SCALAR", None, [times[0]], [times[-1]]
            )
            trans_by_t = [trans_attr.Get(Usd.TimeCode(t)) if trans_attr else None for t in samples]
            rot_by_t = [rot_attr.Get(Usd.TimeCode(t)) if rot_attr else None for t in samples]
            scale_by_t = [scale_attr.Get(Usd.TimeCode(t)) if scale_attr else None for t in samples]
            gltf_samplers = []
            gltf_channels = []

            def add_track(node_i: int, path: str, values: list[float], typ: str) -> None:
                acc = binary.accessor(pack_f32(values), "f", len(samples), typ, None)
                sampler_i = len(gltf_samplers)
                gltf_samplers.append({"input": time_acc, "output": acc, "interpolation": "LINEAR"})
                gltf_channels.append({"sampler": sampler_i, "target": {"node": node_i, "path": path}})

            for anim_i, joint_path in enumerate(anim_joints):
                skel_i = joint_index.get(joint_path)
                if skel_i is None:
                    name = joint_path.split("/")[-1]
                    matches = [i for i, n in enumerate(joint_names) if n == name]
                    skel_i = matches[0] if len(matches) == 1 else None
                if skel_i is None:
                    continue
                node_i = joint_node[skel_i]
                bind_trs = trs_from_matrix(rest_xfs[skel_i])
                trans_out: list[float] = []
                rot_out: list[float] = []
                scale_out: list[float] = []
                has_trans = False
                has_rot = False
                prev_q = None
                for si in range(len(samples)):
                    tr = trans_by_t[si]
                    if tr is not None and anim_i < len(tr):
                        v = tr[anim_i]
                        trans_out.extend((float(v[0]), float(v[1]), float(v[2])))
                        has_trans = True
                    else:
                        trans_out.extend(bind_trs["translation"])
                    rq = rot_by_t[si]
                    if rq is not None and anim_i < len(rq):
                        q = rq[anim_i]
                        imag = q.GetImaginary()
                        quat = [float(imag[0]), float(imag[1]), float(imag[2]), float(q.GetReal())]
                        nrm = (quat[0] ** 2 + quat[1] ** 2 + quat[2] ** 2 + quat[3] ** 2) ** 0.5 or 1.0
                        quat = [c / nrm for c in quat]
                        if prev_q is not None and sum(a * b for a, b in zip(prev_q, quat)) < 0:
                            quat = [-c for c in quat]
                        prev_q = quat
                        rot_out.extend(quat)
                        has_rot = True
                    else:
                        rot_out.extend(bind_trs["rotation"])
                    sc = scale_by_t[si]
                    if sc is not None and anim_i < len(sc):
                        v = sc[anim_i]
                        scale_out.extend((float(v[0]), float(v[1]), float(v[2])))
                    else:
                        scale_out.extend(bind_trs["scale"])
                if has_trans:
                    add_track(node_i, "translation", trans_out, "VEC3")
                if has_rot:
                    add_track(node_i, "rotation", rot_out, "VEC4")
                if scale_out and any(abs(v - 1.0) > 1e-3 for v in scale_out):
                    add_track(node_i, "scale", scale_out, "VEC3")
            if gltf_channels:
                animations.append({"name": "wave", "samplers": gltf_samplers, "channels": gltf_channels})

    extra = (4 - (len(binary.buf) % 4)) % 4
    binary.buf.extend(b"\x00" * extra)

    gltf = {
        "asset": {"version": "2.0", "generator": "bondwatch-usd-to-glb"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": meshes,
        "skins": [skin],
        "materials": materials,
        "animations": animations,
        "buffers": [{"byteLength": len(binary.buf)}],
        "bufferViews": binary.views,
        "accessors": binary.accessors,
    }
    if images:
        gltf["images"] = images
        gltf["textures"] = textures
        gltf["samplers"] = samplers

    json_bytes = pad_json(json.dumps(gltf, separators=(",", ":")).encode("utf-8"))
    total = 12 + 8 + len(json_bytes) + 8 + len(binary.buf)
    out_glb.parent.mkdir(parents=True, exist_ok=True)
    with out_glb.open("wb") as fh:
        fh.write(struct.pack("<4sII", b"glTF", 2, total))
        fh.write(struct.pack("<II", len(json_bytes), JSON_CHUNK))
        fh.write(json_bytes)
        fh.write(struct.pack("<II", len(binary.buf), BIN_CHUNK))
        fh.write(binary.buf)
    nchan = len(animations[0]["channels"]) if animations else 0
    print(
        f"wrote {out_glb} ({out_glb.stat().st_size} bytes) "
        f"joints={len(joint_paths)} meshes={len(meshes)} wave_channels={nchan}"
    )


if __name__ == "__main__":
    import zipfile
    import tempfile

    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
        r"c:\Users\123\Documents\WXWork\1688856493838604\Cache\File\2026-08\ARFile.zip"
    )
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("D:/Work/BondWatch/cloud/static/watch/models/lumi.glb")
    usdz = src
    if src.suffix.lower() == ".zip":
        tmp = Path(tempfile.mkdtemp(prefix="arfile_"))
        with zipfile.ZipFile(src) as zf:
            zf.extractall(tmp)
        usdz = tmp / "ARFile" / "lumi.usdz"
    convert(usdz, dst)
    boy = dst.with_name("boybase.glb")
    boy.write_bytes(dst.read_bytes())
    print(f"copied {boy}")
