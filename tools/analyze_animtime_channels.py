import bpy
import math


START, END = 278, 1200
scene = bpy.context.scene
armature = bpy.data.objects["viewarms_c_usa_mp_seal6_longsleeve_viewhands"]
camera = bpy.data.objects["POV"]


def sample(frame):
    scene.frame_set(frame)
    torso = armature.pose.bones["tag_torso"].matrix_basis.copy()
    view = armature.pose.bones["tag_view"].matrix_basis.copy()
    camera_bone = armature.pose.bones["tag_camera"].matrix_basis.copy()
    return torso, view, camera_bone, camera.data.lens


def qangle(a, b):
    return abs(a.to_quaternion().rotation_difference(b.to_quaternion()).angle)


samples = {frame: sample(frame) for frame in range(START, END + 1)}


def intervals(predicate):
    changed = [frame for frame in range(START, END) if predicate(samples[frame], samples[frame + 1])]
    if not changed:
        return []
    result = []
    begin = previous = changed[0]
    for frame in changed[1:]:
        if frame > previous + 1:
            result.append((begin, previous + 1))
            begin = frame
        previous = frame
    result.append((begin, previous + 1))
    return result


channels = {
    "ADS_TAG_TORSO": lambda a, b: (a[0].translation - b[0].translation).length > 1e-4 or qangle(a[0], b[0]) > math.radians(0.01),
    "VIEW_POSITION": lambda a, b: (a[1].translation - b[1].translation).length > 1e-3 or qangle(a[1], b[1]) > math.radians(0.05),
    "AUTHORED_CAMERA_BONE": lambda a, b: (a[2].translation - b[2].translation).length > 1e-5 or qangle(a[2], b[2]) > math.radians(0.01),
    "FOV_LENS": lambda a, b: abs(a[3] - b[3]) > 1e-4,
}

print("CHANNEL_INTERVALS_BEGIN")
for name, predicate in channels.items():
    print(name)
    for begin, end in intervals(predicate):
        a, b = samples[begin], samples[end]
        index = {"ADS_TAG_TORSO": 0, "VIEW_POSITION": 1, "AUTHORED_CAMERA_BONE": 2}.get(name)
        if index is None:
            detail = f"{a[3]:.4f}->{b[3]:.4f}"
        else:
            av, bv = a[index].translation, b[index].translation
            detail = f"T({av.x:.4f},{av.y:.4f},{av.z:.4f})->({bv.x:.4f},{bv.y:.4f},{bv.z:.4f}) R={math.degrees(qangle(a[index], b[index])):.3f}deg"
        print(f"  {begin}-{end} ({(end-begin)/50.0:.3f}s) {detail}")
print("CHANNEL_INTERVALS_END")
