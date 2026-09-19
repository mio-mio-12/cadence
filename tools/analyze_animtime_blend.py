import bpy
import json


def curve_summary(curve):
    points = curve.keyframe_points
    frames = [point.co.x for point in points]
    return {
        "path": curve.data_path,
        "index": curve.array_index,
        "keys": len(points),
        "first": min(frames) if frames else None,
        "last": max(frames) if frames else None,
        "interpolation": sorted({point.interpolation for point in points}),
    }


result = {
    "scenes": [],
    "objects": [],
    "actions": [],
}

for scene in bpy.data.scenes:
    result["scenes"].append({
        "name": scene.name,
        "fps": scene.render.fps,
        "fps_base": scene.render.fps_base,
        "frame_start": scene.frame_start,
        "frame_end": scene.frame_end,
        "frame_current": scene.frame_current,
        "markers": [{"name": marker.name, "frame": marker.frame} for marker in scene.timeline_markers],
    })

for obj in bpy.data.objects:
    animation = obj.animation_data
    entry = {
        "name": obj.name,
        "type": obj.type,
        "parent": obj.parent.name if obj.parent else None,
        "parent_type": obj.parent_type,
        "parent_bone": obj.parent_bone,
        "action": animation.action.name if animation and animation.action else None,
        "nla": [],
        "custom_properties": {key: repr(obj[key])[:300] for key in obj.keys() if key != "_RNA_UI"},
    }
    if obj.type == "ARMATURE":
        entry["bones"] = [bone.name for bone in obj.data.bones]
    if animation:
        for track in animation.nla_tracks:
            entry["nla"].append({
                "name": track.name,
                "mute": track.mute,
                "solo": track.is_solo,
                "strips": [{
                    "name": strip.name,
                    "action": strip.action.name if strip.action else None,
                    "frame_start": strip.frame_start,
                    "frame_end": strip.frame_end,
                    "action_frame_start": strip.action_frame_start,
                    "action_frame_end": strip.action_frame_end,
                    "blend_type": strip.blend_type,
                    "extrapolation": strip.extrapolation,
                    "influence": strip.influence,
                    "blend_in": strip.blend_in,
                    "blend_out": strip.blend_out,
                } for strip in track.strips],
            })
    result["objects"].append(entry)

for action in bpy.data.actions:
    curves = [curve_summary(curve) for curve in action.fcurves]
    paths = {}
    for curve in curves:
        paths.setdefault(curve["path"], []).append(curve)
    result["actions"].append({
        "name": action.name,
        "frame_range": list(action.frame_range),
        "fcurves": len(curves),
        "groups": [group.name for group in action.groups],
        "paths": paths,
    })

print("ANIMTIME_JSON_BEGIN")
print(json.dumps(result, indent=2))
print("ANIMTIME_JSON_END")
