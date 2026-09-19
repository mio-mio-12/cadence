import bpy
import math
from mathutils import Quaternion


EVENTS = [278, 323, 328, 338, 363, 386, 442, 464, 479, 523, 527, 539, 552,
          568, 593, 616, 663, 758, 783, 870, 873, 887, 913, 923, 940, 949, 951,
          958, 973, 986, 988, 998, 1013, 1023, 1027, 1043, 1116, 1133, 1198]
SEGMENTS = list(zip(EVENTS, EVENTS[1:]))


def rotation_angle(a, b):
    delta = a.to_quaternion().rotation_difference(b.to_quaternion())
    return abs(delta.angle)


def bone_change(armature, start, end, bone_name):
    scene = bpy.context.scene
    scene.frame_set(start)
    a = armature.pose.bones[bone_name].matrix_basis.copy()
    scene.frame_set(end)
    b = armature.pose.bones[bone_name].matrix_basis.copy()
    translation = (a.translation - b.translation).length
    rotation = rotation_angle(a, b)
    return translation, rotation


def pose_step(armature, frame):
    scene = bpy.context.scene
    scene.frame_set(frame)
    before = {bone.name: bone.matrix_basis.copy() for bone in armature.pose.bones}
    scene.frame_set(frame + 1)
    translation = 0.0
    rotation = 0.0
    for bone in armature.pose.bones:
        a = before[bone.name]
        translation += (a.translation - bone.matrix_basis.translation).length_squared
        angle = rotation_angle(a, bone.matrix_basis)
        rotation += angle * angle
    count = max(1, len(before))
    return math.sqrt(translation / count), math.sqrt(rotation / count)


armatures = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
print("DYNAMICS_BEGIN")
print(f"SCENE fps={bpy.context.scene.render.fps / bpy.context.scene.render.fps_base:g} range={bpy.context.scene.frame_start}..{bpy.context.scene.frame_end}")
for armature in armatures:
    action = armature.animation_data.action if armature.animation_data else None
    print(f"ARMATURE {armature.name} bones={len(armature.data.bones)} action={action.name if action else '-'} range={tuple(action.frame_range) if action else '-'}")

main = bpy.data.objects.get("viewarms_c_usa_mp_seal6_longsleeve_viewhands")
if main:
    print("MAIN_SEGMENTS")
    for start, end in SEGMENTS:
        ranked = []
        for bone in main.pose.bones:
            translation, rotation = bone_change(main, start, end, bone.name)
            score = translation + rotation * 8.0
            if score > 1e-5:
                ranked.append((score, bone.name, translation, math.degrees(rotation)))
        ranked.sort(reverse=True)
        mid = (start + end) // 2
        speeds = [(frame, *pose_step(main, frame)) for frame in sorted({start, min(start + 1, end - 1), mid, max(start, end - 2), end - 1}) if frame < end]
        top = ", ".join(f"{name}:T{translation:.3f}/R{rotation:.2f}" for _, name, translation, rotation in ranked[:8])
        speed_text = ", ".join(f"{frame}:{translation:.4f}/{math.degrees(rotation):.3f}" for frame, translation, rotation in speeds)
        print(f"SEG {start}-{end} TOP [{top}] STEP(T/Rdeg) [{speed_text}]")

    watched = [name for name in ["tag_view", "tag_ads", "tag_torso", "tag_camera", "tag_weapon", "j_shoulder_le", "j_shoulder_ri", "j_wrist_le", "j_wrist_ri"] if name in main.pose.bones]
    print("MAIN_EVENT_POSES")
    for frame in EVENTS:
        bpy.context.scene.frame_set(frame)
        values = []
        for name in watched:
            matrix = main.pose.bones[name].matrix_basis
            location = matrix.translation
            rotation = matrix.to_quaternion()
            values.append(f"{name}=T({location.x:.3f},{location.y:.3f},{location.z:.3f}) Q({rotation.w:.4f},{rotation.x:.4f},{rotation.y:.4f},{rotation.z:.4f})")
        print(f"FRAME {frame} " + " | ".join(values))

camera = bpy.data.objects.get("POV")
if camera:
    print("CAMERA_EVENTS")
    for frame in EVENTS:
        bpy.context.scene.frame_set(frame)
        loc = camera.matrix_world.translation
        rot = camera.matrix_world.to_quaternion()
        print(f"CAM {frame} lens={camera.data.lens:.4f} T=({loc.x:.4f},{loc.y:.4f},{loc.z:.4f}) Q=({rot.w:.5f},{rot.x:.5f},{rot.y:.5f},{rot.z:.5f})")
print("DYNAMICS_END")
