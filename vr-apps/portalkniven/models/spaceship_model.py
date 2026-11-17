"""Procedural exterior reconstruction of the supplied retro-futurist spacecraft.
Run with Blender 4.x: blender --background --python spaceship_model.py
Exports spaceship.glb next to this script (or set EXPORT_PATH below).

Coordinate convention: ship nose = -X, tail = +X, up = +Z. Approximate length: 20 m.
This is an editable first-pass reconstruction, not a scan or exact CAD reconstruction.
"""
import bpy, math, os
from mathutils import Vector

EXPORT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "spaceship.glb")

# ---------- scene ----------
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials):
    pass
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.render.engine = 'BLENDER_EEVEE_NEXT'

# ---------- materials ----------
def mat(name, color, metallic=0.0, roughness=0.72):
    m = bpy.data.materials.new(name); m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color, 1)
    bs.inputs['Metallic'].default_value = metallic
    bs.inputs['Roughness'].default_value = roughness
    return m
BLUE = mat('Hull | pale arctic blue', (0.22, 0.58, 0.82), 0.28, 0.58)
BLUE_DARK = mat('Hull | shadow blue', (0.10, 0.34, 0.54), 0.38, 0.62)
ORANGE = mat('Engine grilles | warm copper orange', (0.96, 0.48, 0.20), 0.35, 0.48)
DARK = mat('Recesses | graphite', (0.035, 0.065, 0.085), 0.55, 0.65)
METAL = mat('Mechanical details | muted steel', (0.36, 0.43, 0.47), 0.72, 0.42)

# ---------- helpers ----------
def smooth(obj, material=None):
    if material: obj.data.materials.append(material)
    if obj.type == 'MESH':
        for p in obj.data.polygons: p.use_smooth = True
    return obj

def uv(name, loc, scale, material, seg=24, rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=loc)
    o=bpy.context.object; o.name=name; o.scale=scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return smooth(o, material)

def cube(name, loc, scale, material, bevel=0.12):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc); o=bpy.context.object; o.name=name; o.dimensions=scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod=o.modifiers.new('Soft manufactured edges','BEVEL'); mod.width=bevel; mod.segments=3
        o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    o.data.materials.append(material); return o

def cyl(name, loc, radius, depth, material, axis='X', vertices=20):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    o=bpy.context.object; o.name=name
    if axis=='X': o.rotation_euler[1]=math.pi/2
    elif axis=='Y': o.rotation_euler[0]=math.pi/2
    return smooth(o, material)

def beam(name, a, b, radius, material, vertices=8):
    a,b=Vector(a),Vector(b); d=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=d.length, location=(a+b)/2)
    o=bpy.context.object; o.name=name; o.rotation_euler=d.to_track_quat('Z','Y').to_euler()
    return smooth(o, material)

def loft_x(name, sections, material, n=32):
    """Elliptical loft. sections are (x, half_width_y, half_height_z, z_center)."""
    verts=[]; faces=[]
    for x, ry, rz, zc in sections:
        for j in range(n):
            t=2*math.pi*j/n
            verts.append((x, ry*math.cos(t), zc+rz*math.sin(t)))
    for i in range(len(sections)-1):
        for j in range(n):
            a=i*n+j; b=i*n+(j+1)%n
            faces.append((a,b,b+n,a+n))
    faces += [tuple(range(n-1,-1,-1)), tuple((len(sections)-1)*n+j for j in range(n))]
    me=bpy.data.meshes.new(name+' mesh'); me.from_pydata(verts,[],faces); me.materials.append(material)
    ob=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(ob)
    for p in me.polygons: p.use_smooth=True
    sub=ob.modifiers.new('Hull surface refinement','SUBSURF'); sub.levels=1
    return ob

def panel_curve(name, pts, material=BLUE_DARK, bevel=0.018):
    cu=bpy.data.curves.new(name,'CURVE'); cu.dimensions='3D'; cu.bevel_depth=bevel; cu.bevel_resolution=1
    sp=cu.splines.new('POLY'); sp.points.add(len(pts)-1)
    for p,co in zip(sp.points,pts): p.co=(*co,1)
    ob=bpy.data.objects.new(name,cu); bpy.context.collection.objects.link(ob); ob.data.materials.append(material); return ob

# ---------- primary hull ----------
sections=[(-9.2,0.08,0.10,0),(-8.8,1.25,0.85,0),(-7.8,2.45,1.45,0),(-5.8,3.25,1.9,0),(-2.5,3.65,2.15,0),(1.0,3.55,2.05,0),(4.5,3.0,1.75,0),(7.0,2.15,1.35,0),(8.8,1.15,0.85,0),(9.4,0.12,0.12,0)]
loft_x('01 | Main pressure hull',sections,BLUE,48)
# lower keel and dorsal spine
loft_x('02 | Underside keel',[(-7.5,.35,.32,-1.35),(-5,.52,.42,-1.48),(0,.48,.40,-1.45),(5,.34,.30,-1.15),(7,.12,.12,-.95)],BLUE_DARK,20)
loft_x('03 | Dorsal spine',[(-4,.18,.18,1.55),(-1,.42,.38,1.85),(2,.55,.52,1.95),(5,.38,.36,1.65),(7,.12,.12,1.35)],BLUE,24)

# ---------- four external engine / radiator pods ----------
# Two port/starboard pairs, vertically staggered. Their copper faces point toward the nose (-X).
for side,label in [(-1,'Port'),(1,'Starboard')]:
    for level,z in [(-1,-1.25),(1,1.25)]:
        y=side*3.55
        pod_sections=[(-4.6,.10,.12,z),(-4.25,.72,.72,z),(-3.45,.82,.78,z),(-1.0,.78,.72,z),(-.35,.55,.55,z),(0,.12,.14,z)]
        # offset the pod's cross-section in Y using a translated mesh after loft
        ob=loft_x(f'10 | {label} engine pod {level:+d}',pod_sections,BLUE,28)
        ob.location.y=y
        # copper inset on front face, dark backing, slatted grille
        cube(f'{label} {level} | grille dark recess',(-4.32,y,z),(0.10,1.08,1.02),DARK,.10)
        cube(f'{label} {level} | copper grille surround',(-4.40,y,z),(0.12,1.02,.96),ORANGE,.10)
        cube(f'{label} {level} | grille inset',(-4.475,y,z),(0.035,.84,.76),DARK,.035)
        for k in range(6):
            zz=z-.31+k*.124
            cube(f'{label} {level} | grille slat {k}',(-4.51,y,zz),(.045,.78,.035),ORANGE,.012)
        for yy in [-.27,-.09,.09,.27]:
            for zz in [-.19,.19]:
                uv(f'{label} {level} | nozzle detail',(-4.54,y+yy,z+zz),(.055,.055,.055),METAL,12,8)
        # external pod collar and small side access plate
        cube(f'{label} {level} | access plate',(-1.8,y+side*.79,z),(.95,.055,.46),BLUE_DARK,.08)
        for k in range(3): cube(f'{label} {level} | access rib {k}',(-2.1+k*.28,y+side*.83,z),(.045,.025,.32),METAL,.01)

# ---------- central tower and antenna ----------
uv('20 | Tower shoulder',(1.8,0,2.05),(1.55,1.35,1.25),BLUE)
loft_x('21 | Tower neck',[(.9,.85,.65,2.5),(1.5,1.0,.9,2.75),(2.3,.9,.82,2.8),(2.9,.58,.55,2.85)],BLUE,32)
# stacked tower housings
for z, sx, sy, sz in [(3.15,1.25,1.35,.52),(3.65,1.0,1.12,.48),(4.10,.72,.82,.38),(4.43,.48,.56,.28)]:
    cube(f'22 | Tower armor tier {z}',(2.05,0,z),(sx,sy,sz),BLUE,.18)
# antenna mast
beam('23 | Main antenna mast',(2.05,0,4.55),(2.05,0,7.0),.055,BLUE,10)
beam('23 | Antenna crossbar',(2.05,-.42,6.35),(2.05,.42,6.35),.035,METAL,8)
uv('23 | Antenna tip',(2.05,0,7.02),(.09,.09,.16),BLUE,12,8)
# tower side sensors
for side in [-1,1]:
    beam('Tower lateral aerial',(1.75,side*.72,3.75),(1.75,side*1.18,3.9),.035,METAL,8)

# ---------- fins, pylons and stabilizers ----------
def fin_mesh(name, verts, material):
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],[(0,1,2,3)]); me.materials.append(material)
    ob=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(ob)
    sol=ob.modifiers.new('Fin thickness','SOLIDIFY'); sol.thickness=.12
    bev=ob.modifiers.new('Fin edge softening','BEVEL'); bev.width=.07; bev.segments=2
    return ob
for side in [-1,1]:
    fin_mesh(f'30 | Lower swept stabilizer {side}',[(-5.8,side*1.1,-1.0),(-3.0,side*1.5,-1.3),(-1.7,side*2.0,-3.15),(-3.8,side*1.6,-2.45)],BLUE)
    fin_mesh(f'31 | Side swept fin {side}',[(1.0,side*2.7,.15),(4.6,side*3.0,.35),(6.4,side*3.6,1.15),(3.2,side*3.0,1.0)],BLUE)
    beam('Fin support strut',(-3.3,side*1.3,-1.1),(-2.7,side*1.75,-2.0),.11,BLUE_DARK,10)

# ---------- surface armor panels and mechanical recesses ----------
# Curved panel seams following the upper hull surface; deliberately restrained for a clean VR silhouette.
for x in [-6.8,-5.1,-3.2,-1.0,1.2,3.4,5.4,7.0]:
    # transverse seam approximated over visible upper surface
    ry=3.55 if x<2 else max(1.3,3.55-(x-2)*.45)
    rz=2.0 if x<2 else max(.8,2.0-(x-2)*.22)
    pts=[]
    for j in range(17):
        y=-ry+2*ry*j/16
        z=rz*math.sqrt(max(.02,1-(y/ry)**2))+.025
        pts.append((x,y,z))
    panel_curve(f'40 | Hull transverse seam {x}',pts,BLUE_DARK,.012)
# long raised armor bands along upper flanks
for side in [-1,1]:
    for zoff in [.55,1.05]:
        pts=[]
        for i in range(30):
            x=-6.8+i*13.2/29
            ry=3.5 if x<1 else max(1.3,3.5-(x-1)*.40)
            rz=2.0 if x<1 else max(.8,2.0-(x-1)*.18)
            y=side*ry*.72
            z=rz*math.sqrt(1-(.72**2))+zoff*.10
            pts.append((x,y,z))
        panel_curve(f'41 | Longitudinal armor seam {side} {zoff}',pts,BLUE_DARK,.018)
# dark service recesses with repeated mechanical blocks, mirrored
for side in [-1,1]:
    for x in [-2.8,.2,3.0]:
        y=side*2.72; z=.75
        cube('42 | Recess shadow',(x,y,z),(1.25,.09,.55),DARK,.07)
        for k in range(5):
            cube('42 | Mechanical block',(x-.48+k*.23,y+side*.07,z),(.14,.08,.30),METAL,.035)
            uv('42 | Fastener',(x-.48+k*.23,y+side*.12,z+.20),(.035,.025,.035),ORANGE,10,6)
# small hull rivets along shoulders
for side in [-1,1]:
    for x in [-6,-4.5,-3,-1.5,0,1.5,3,4.5,6]:
        uv('43 | Hull rivet',(x,side*2.55,1.18),(.045,.045,.045),METAL,10,6)

# ---------- rear service details ----------
for side in [-1,1]:
    for z in [-.55,.0,.55]:
        cube('50 | Aft vent',(7.9,side*1.0,z),(.08,.72,.12),DARK,.025)
        cube('50 | Aft vent lip',(7.84,side*1.0,z+.09),(.12,.78,.035),METAL,.01)

# ---------- presentation defaults / export ----------
# Origin at ship center; apply transforms to keep the GLB predictable.
for obj in bpy.context.scene.objects:
    if obj.type == 'MESH':
        # keep bevel/subsurf modifiers live in Blender; glTF exporter evaluates them
        pass
# Select all for export
bpy.ops.object.select_all(action='SELECT')
bpy.context.view_layer.objects.active=bpy.context.selected_objects[0]
os.makedirs(os.path.dirname(EXPORT_PATH), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=EXPORT_PATH, export_format='GLB', use_selection=True,
                          export_apply=True, export_yup=True, export_materials='EXPORT')
print('Exported WebXR-ready GLB:', EXPORT_PATH)
