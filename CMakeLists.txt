cmake_minimum_required(VERSION 3.24)

project(Cadence VERSION 0.9.0 LANGUAGES C CXX)

# Numbered builds remain runnable with only their adjacent asset folders.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CAST_BUILD_UI "Build the Dear ImGui desktop viewer" ON)
option(CAST_BUILD_TESTS "Build parser tests" ON)

add_library(cast_core
    src/cast/CastDocument.cpp
    src/cast/CastDocument.h
)
add_executable(bot_bind_audit EXCLUDE_FROM_ALL tools/BotBindAudit.cpp)
target_link_libraries(bot_bind_audit PRIVATE cast_scene cast_assets)
target_include_directories(cast_core PUBLIC src)
target_compile_features(cast_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(cast_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(cast_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(cast_scene
    src/scene/Source2Retarget.cpp
    src/scene/CastScene.cpp
    src/scene/T6Magazine.cpp
    src/scene/CastScene.h
    src/scene/Math3D.h
)
target_include_directories(cast_scene PUBLIC src)
target_link_libraries(cast_scene PUBLIC cast_core)
if(MSVC)
    target_compile_options(cast_scene PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(cast_scene PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_shoulder tools/TestShoulder.cpp)
add_executable(practice_tools_tests tests/PracticeToolsTests.cpp)
target_include_directories(practice_tools_tests PRIVATE src)
add_test(NAME practice_tools_tests COMMAND practice_tools_tests)
add_executable(pointblank_probe EXCLUDE_FROM_ALL tools/PointBlankProbe.cpp)
add_executable(imported_game_audit EXCLUDE_FROM_ALL tools/ImportedGameAudit.cpp)
add_executable(new_imports_audit EXCLUDE_FROM_ALL tools/NewImportsAudit.cpp)
target_link_libraries(new_imports_audit PRIVATE cast_scene cast_assets cast_core)
add_executable(new_import_policy_tests tests/NewImportPolicyTests.cpp)
target_link_libraries(new_import_policy_tests PRIVATE cast_assets cast_scene)
add_test(NAME new_import_policy_tests COMMAND new_import_policy_tests)
add_executable(environment_source_tests tests/EnvironmentSourceTests.cpp)
target_include_directories(environment_source_tests PRIVATE src)
add_test(NAME environment_source_tests COMMAND environment_source_tests)
add_executable(weapon_turn_tests tests/WeaponTurnTests.cpp)
target_link_libraries(weapon_turn_tests PRIVATE cast_scene)
add_test(NAME weapon_turn_tests COMMAND weapon_turn_tests)
add_executable(heaven_smile_tests tests/HeavenSmileTests.cpp)
target_link_libraries(heaven_smile_tests PRIVATE cast_scene)
add_test(NAME heaven_smile_tests COMMAND heaven_smile_tests)
target_link_libraries(imported_game_audit PRIVATE cast_scene cast_assets cast_core)
add_executable(imported_body_tests tests/ImportedBodyTests.cpp)
target_link_libraries(imported_body_tests PRIVATE cast_scene)
add_test(NAME imported_body_tests COMMAND imported_body_tests)
add_executable(imported_view_retarget_tests tests/ImportedViewRetargetTests.cpp)
add_executable(pose_only_chain_tests tests/PoseOnlyChainTests.cpp)
target_include_directories(pose_only_chain_tests PRIVATE src)
add_test(NAME pose_only_chain_tests COMMAND pose_only_chain_tests)
add_executable(pose_only_runtime_tests tests/PoseOnlyRuntimeTests.cpp)
target_link_libraries(pose_only_runtime_tests PRIVATE cast_scene)
add_test(NAME pose_only_runtime_tests COMMAND pose_only_runtime_tests)
target_link_libraries(imported_view_retarget_tests PRIVATE cast_scene)
add_test(NAME imported_view_retarget_tests COMMAND imported_view_retarget_tests)
add_executable(bind_baked_body_delta_tests tests/BindBakedBodyDeltaTests.cpp)
target_link_libraries(bind_baked_body_delta_tests PRIVATE cast_scene)
add_test(NAME bind_baked_body_delta_tests COMMAND bind_baked_body_delta_tests)
add_executable(character_collision_tests tests/CharacterCollisionTests.cpp)
target_link_libraries(character_collision_tests PRIVATE cast_scene cast_glb_map)
add_test(NAME character_collision_tests COMMAND character_collision_tests)
add_executable(proximal_support_tests tests/ProximalSupportTests.cpp)
target_link_libraries(proximal_support_tests PRIVATE cast_scene)
add_test(NAME proximal_support_tests COMMAND proximal_support_tests)
add_executable(proximal_continuation_tests tests/ProximalContinuationTests.cpp)
target_link_libraries(proximal_continuation_tests PRIVATE cast_scene)
add_test(NAME proximal_continuation_tests COMMAND proximal_continuation_tests)
add_executable(imported_world_grip_tests tests/ImportedWorldGripTests.cpp)
add_executable(imported_muzzle_audit EXCLUDE_FROM_ALL tools/ImportedMuzzleAudit.cpp)
target_link_libraries(imported_muzzle_audit PRIVATE cast_scene)
target_link_libraries(imported_world_grip_tests PRIVATE cast_scene)
add_test(NAME imported_world_grip_tests COMMAND imported_world_grip_tests)
add_executable(imported_body_retarget_audit EXCLUDE_FROM_ALL tools/ImportedBodyRetargetAudit.cpp)
target_link_libraries(imported_body_retarget_audit PRIVATE cast_scene)
add_executable(pointblank_matrix EXCLUDE_FROM_ALL tools/PointBlankMatrix.cpp)
target_link_libraries(pointblank_matrix PRIVATE cast_scene cast_assets cast_weapon)
target_link_libraries(pointblank_probe PRIVATE cast_scene cast_core)
target_link_libraries(test_shoulder PRIVATE cast_scene cast_core)
add_executable(cs2_retarget_diagnostics EXCLUDE_FROM_ALL tools/Cs2RetargetDiagnostics.cpp)
target_link_libraries(cs2_retarget_diagnostics PRIVATE cast_scene cast_core)
add_executable(runtime_pose_adapter_diagnostics EXCLUDE_FROM_ALL tools/RuntimePoseAdapterDiagnostics.cpp)
target_link_libraries(runtime_pose_adapter_diagnostics PRIVATE cast_scene cast_core)
add_executable(test_scan tools/test_scan.cpp)
target_link_libraries(test_scan PRIVATE cast_assets cast_weapon cast_scene cast_core)

add_library(cast_take
    src/content/DemoSets.cpp
    src/take/DollyCamera.cpp
    src/take/DollyCamera.h
    src/take/Take.cpp
    src/take/Take.h
)
target_include_directories(cast_take PUBLIC src)
add_library(cadence_zip STATIC external/miniz/miniz.c external/miniz/miniz_tdef.c external/miniz/miniz_tinfl.c external/miniz/miniz_zip.c)
target_include_directories(cadence_zip PUBLIC external/miniz)
target_compile_definitions(cadence_zip PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(cast_take PRIVATE cadence_zip)
add_executable(demo_sets_tests tests/DemoSetsTests.cpp)
target_link_libraries(demo_sets_tests PRIVATE cast_take cadence_zip)
add_test(NAME demo_sets_tests COMMAND demo_sets_tests)
target_link_libraries(cast_take PUBLIC cast_scene)
if(MSVC)
    target_compile_options(cast_take PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(cast_take PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(cast_assets
    src/assets/AssetCatalog.cpp
    src/assets/AssetCatalog.h
)
target_include_directories(cast_assets PUBLIC src)
target_link_libraries(cast_assets PUBLIC cast_core)
if(MSVC)
    target_compile_options(cast_assets PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(cast_assets PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(cast_weapon
    src/weapon/WeaponProfile.cpp
    src/weapon/WeaponProfile.h
)
target_include_directories(cast_weapon PUBLIC src)
target_link_libraries(cast_weapon PUBLIC cast_scene)
if(MSVC)
    target_compile_options(cast_weapon PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(cast_weapon PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CAST_BUILD_UI)
    include(FetchContent)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    FetchContent_Declare(box3d
        GIT_REPOSITORY https://github.com/erincatto/box3d.git
        GIT_TAG 336263320469e5e9febfe18697ab562488f7e97a
    )
    FetchContent_MakeAvailable(box3d)

    set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/libwebp EXCLUDE_FROM_ALL)

    add_library(cast_glb_map
        src/scene/GlbMap.cpp
        src/scene/AuthoredGameplay.cpp
        src/scene/GlbMap.h
        src/scene/C2MMap.cpp
        src/scene/C2mxMap.cpp
        src/scene/CodmMaterialMetadata.cpp
        src/scene/C2MMap.h
    )
    target_include_directories(cast_glb_map PUBLIC src PRIVATE external/cgltf)
    target_link_libraries(cast_glb_map PUBLIC cast_scene)

    add_library(imgui_backend
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_backend PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_backend PUBLIC glfw)

    find_package(OpenGL REQUIRED)
    add_library(stage_renderer
        src/render/GlApi.cpp
        src/render/GlApi.h
        src/render/DdsLoader.cpp
        src/render/DdsLoader.h
        src/render/PreparedTexture.cpp
        src/render/PreparedTexture.h
        src/render/StageRenderer.cpp
        src/render/StageRenderer.h
        src/render/NativeReShade.cpp
    )
    target_include_directories(stage_renderer PUBLIC src PRIVATE external/libwebp/src)
    target_link_libraries(stage_renderer PUBLIC cast_scene glfw OpenGL::GL webpdecoder)
    add_library(native_reshade INTERFACE)
    add_executable(native_reshade_audit EXCLUDE_FROM_ALL tools/NativeReShadeAudit.cpp)
    target_link_libraries(native_reshade_audit PRIVATE native_reshade)
    target_include_directories(stage_renderer PRIVATE external/reshade-6.8.0)
    target_link_libraries(native_reshade INTERFACE stage_renderer)
    if(WIN32)
        target_link_libraries(stage_renderer PUBLIC d3d11 dxgi)
    endif()
    if(WIN32)
        target_link_libraries(stage_renderer PUBLIC windowscodecs ole32)
    endif()
    add_executable(runtime_pose_visual_regression EXCLUDE_FROM_ALL tools/RuntimePoseVisualRegression.cpp)
    add_executable(eye_overlay_visual_check EXCLUDE_FROM_ALL tools/EyeOverlayVisualCheck.cpp)
    add_executable(worldmodel_visual_check EXCLUDE_FROM_ALL tools/WorldmodelVisualCheck.cpp)
    target_link_libraries(worldmodel_visual_check PRIVATE cast_core cast_scene cast_take stage_renderer glfw OpenGL::GL)
    target_link_libraries(eye_overlay_visual_check PRIVATE cast_core cast_scene cast_take stage_renderer glfw OpenGL::GL)
    add_executable(animation_mechanism_report EXCLUDE_FROM_ALL tools/AnimationMechanismReport.cpp)
    target_link_libraries(animation_mechanism_report PRIVATE cast_core cast_scene)
    add_executable(t6_magazine_probe EXCLUDE_FROM_ALL tools/T6MagazineProbe.cpp)
    target_link_libraries(t6_magazine_probe PRIVATE cast_core cast_scene)
    target_link_libraries(runtime_pose_visual_regression PRIVATE cast_core cast_scene cast_take stage_renderer glfw OpenGL::GL)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_inspect.cpp")
        add_executable(test_inspect EXCLUDE_FROM_ALL tools/test_inspect.cpp)
        target_link_libraries(test_inspect PRIVATE cast_core cast_scene)
    endif()
    add_executable(cast_viewer src/app/main.cpp)
    target_link_libraries(cast_viewer PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v12 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v12 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v13 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v13 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v14 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v14 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v15 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_definitions(Cadence_v15 PRIVATE CADENCE_RUNTIME_POSE_ADAPTER=1)
    target_link_libraries(Cadence_v15 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v16 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v16 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v17 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v17 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v18 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v18 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v19 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v19 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v20 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v20 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v21 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v21 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v22 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v22 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v23 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v23 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v24 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v24 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v25 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v25 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v26 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v26 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v27 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v27 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v28 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v28 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v29 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v29 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v30 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v30 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v31 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v31 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v32 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v32 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v33 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v33 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v34 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v34 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v35 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v35 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v36 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v36 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v37 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v37 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v38 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v38 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v39 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v39 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v40 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v41 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v41 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v42 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v42 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v43 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v43 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v44 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v44 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v45 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v45 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v46 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v46 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v47 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v47 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v48 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v48 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v49 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v49 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v50 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v50 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v51 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v51 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v52 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v52 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v53 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v53 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v54 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v54 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v55 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v55 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v56 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v56 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v57 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v57 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v58 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v58 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v59 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v59 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v60 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v60 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v61 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v62 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v63 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v64 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v64 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v64 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v64> ${CMAKE_SOURCE_DIR}/Cadence_v64.exe)
    add_executable(Cadence_v65 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v65 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v65 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v65> ${CMAKE_SOURCE_DIR}/Cadence_v65.exe)
    add_executable(Cadence_v66 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v66 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v66 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v66> ${CMAKE_SOURCE_DIR}/Cadence_v66.exe)
    add_executable(Cadence_v67 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v67 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v67 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v67> ${CMAKE_SOURCE_DIR}/Cadence_v67.exe)
    add_executable(Cadence_v68 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v69 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v69 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v69 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v69> ${CMAKE_SOURCE_DIR}/Cadence_v69.exe)
    add_executable(Cadence_v70 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v70 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v70 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v70> ${CMAKE_SOURCE_DIR}/Cadence_v70.exe)
    add_executable(Cadence_v71 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v71 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v71 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v71> ${CMAKE_SOURCE_DIR}/Cadence_v71.exe)
    add_executable(Cadence_v72 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v72 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v72 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v72> ${CMAKE_SOURCE_DIR}/Cadence_v72.exe)
    add_executable(Cadence_v73 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v74 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v75 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v76 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v77 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v77 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v77 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v77> ${CMAKE_SOURCE_DIR}/Cadence_v77.exe)
    add_executable(Cadence_v78 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v78 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v78 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v78> ${CMAKE_SOURCE_DIR}/Cadence_v78.exe)
    add_executable(Cadence_v79 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v79 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v79 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v79> ${CMAKE_SOURCE_DIR}/Cadence_v79.exe)
    add_executable(Cadence_v80 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v80 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v80 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v80> ${CMAKE_SOURCE_DIR}/Cadence_v80.exe)
    add_executable(Cadence_v81 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v81 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v81 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v81> ${CMAKE_SOURCE_DIR}/Cadence_v81.exe)
    add_executable(Cadence_v82 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v82 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v82 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v82> ${CMAKE_SOURCE_DIR}/Cadence_v82.exe)
    add_executable(Cadence_v83 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v83 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v83 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v83> ${CMAKE_SOURCE_DIR}/Cadence_v83.exe)
    add_executable(Cadence_v84 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v84 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v84 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v84> ${CMAKE_SOURCE_DIR}/Cadence_v84.exe)
    add_executable(Cadence_v85 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v85 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v85 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v85> ${CMAKE_SOURCE_DIR}/Cadence_v85.exe)
    add_executable(Cadence_v86 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v86 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v86 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v86> ${CMAKE_SOURCE_DIR}/Cadence_v86.exe)
    add_executable(Cadence_v87 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v87 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v87 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v87> ${CMAKE_SOURCE_DIR}/Cadence_v87.exe)
    add_executable(Cadence_v88 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v88 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v88 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v88> ${CMAKE_SOURCE_DIR}/Cadence_v88.exe)
    add_executable(Cadence_v89 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v89 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v89 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v89> ${CMAKE_SOURCE_DIR}/Cadence_v89.exe)
    add_executable(Cadence_v90 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v90 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v91 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v91 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v92 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v92 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v93 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v93 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v94 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v94 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v95 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v95 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v96 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v96 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v97 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v97 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v98 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v98 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v98 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v98> ${CMAKE_SOURCE_DIR}/Cadence_v98.exe)
    add_executable(Cadence_v99 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v99 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v99 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v99> ${CMAKE_SOURCE_DIR}/Cadence_v99.exe)
    add_executable(Cadence_v100 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v100 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v100 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v100> ${CMAKE_SOURCE_DIR}/Cadence_v100.exe)

    add_executable(Cadence_v101 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v101 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v101 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v101> ${CMAKE_SOURCE_DIR}/Cadence_v101.exe)
    add_executable(Cadence_v102 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v102 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v102 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v102> ${CMAKE_SOURCE_DIR}/Cadence_v102.exe)
    add_executable(Cadence_v103 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v103 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v103 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v103> ${CMAKE_SOURCE_DIR}/Cadence_v103.exe)
    add_executable(Cadence_v104 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v104 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v105 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v105 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v106 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v106 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v107 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v107 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v108 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v108 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v109 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v109 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v110 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v110 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v111 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v111 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v112 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v112 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v113 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v113 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v114 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v114 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v115 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v115 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v116 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v116 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v117 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v117 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v118 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v118 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v119 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v119 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v120 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v120 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v121 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v121 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v122 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v123 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v124 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v127 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v127 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v127 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v127> ${CMAKE_SOURCE_DIR}/Cadence_v127.exe)
    add_executable(Cadence_v128 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v128 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v128 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v128> ${CMAKE_SOURCE_DIR}/Cadence_v128.exe)
    add_executable(Cadence_v129 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v129 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v129 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v129> ${CMAKE_SOURCE_DIR}/Cadence_v129.exe)
    add_executable(Cadence_v130 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v130 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v130 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v130> ${CMAKE_SOURCE_DIR}/Cadence_v130.exe)
    add_executable(Cadence_v131 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v131 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v131 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v131> ${CMAKE_SOURCE_DIR}/Cadence_v131.exe)
    add_executable(Cadence_v132 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v132 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v132 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v132> ${CMAKE_SOURCE_DIR}/Cadence_v132.exe)
    add_executable(Cadence_v134 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v135 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v135 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v135 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v135> ${CMAKE_SOURCE_DIR}/Cadence_v135.exe)
    add_executable(Cadence_v136 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v136 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v136 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v136> ${CMAKE_SOURCE_DIR}/Cadence_v136.exe)
    add_executable(Cadence_v137 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v137 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v137 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v137> ${CMAKE_SOURCE_DIR}/Cadence_v137.exe)
    add_executable(Cadence_v138 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v138 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v139 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v139 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v140 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v140 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v141 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v141 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_options(cast_viewer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v141 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v142 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v143 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v144 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v145 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v146 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v147 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v148 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v149 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v150 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v151 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v152 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v153 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v154 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v155 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v156 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v157 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v158 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v159 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v160 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v161 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v161 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v161 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v162 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v162 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v162 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v163 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v163 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v163 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v164 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v164 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v164 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v164 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v164>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v164> ${CMAKE_SOURCE_DIR}/Cadence_v164.exe)
    add_executable(water_visual_check EXCLUDE_FROM_ALL tools/WaterVisualCheck.cpp)
    add_executable(dof_numeric_check EXCLUDE_FROM_ALL tools/DofNumericCheck.cpp)
    add_executable(rain_motion_audit EXCLUDE_FROM_ALL tools/RainMotionAudit.cpp)
    target_link_libraries(rain_motion_audit PRIVATE stage_renderer cast_take cast_glb_map)
    target_link_libraries(dof_numeric_check PRIVATE stage_renderer cast_take cast_glb_map)
    add_executable(rain_visual_check EXCLUDE_FROM_ALL tools/RainVisualCheck.cpp)
    add_executable(weather_visual_check EXCLUDE_FROM_ALL tools/WeatherVisualCheck.cpp)
    add_executable(bloom_safety_check tools/BloomSafetyCheck.cpp)
    target_link_libraries(bloom_safety_check PRIVATE stage_renderer glfw OpenGL::GL)
    add_test(NAME bloom_safety_check COMMAND bloom_safety_check)
    target_link_libraries(weather_visual_check PRIVATE stage_renderer cast_take cast_glb_map)
    add_executable(weather_tests tests/WeatherTests.cpp)
    target_include_directories(weather_tests PRIVATE src)
    add_test(NAME weather_tests COMMAND weather_tests)
    add_executable(rain_app_check EXCLUDE_FROM_ALL tools/RainAppCheck.cpp)
    add_executable(rain_preset_audit EXCLUDE_FROM_ALL tools/RainPresetAudit.cpp)
    target_compile_options(rain_preset_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(rain_preset_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    target_compile_options(rain_app_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(rain_app_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(codm_runtime_audit EXCLUDE_FROM_ALL tools/CodmRuntimeAudit.cpp)
    add_executable(codm_bot_audit EXCLUDE_FROM_ALL tools/CodmBotAudit.cpp)
    target_compile_options(codm_bot_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(codm_bot_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(codm_runtime_audit PRIVATE cast_scene cast_weapon cast_take cast_glb_map stage_renderer)
    add_executable(Cadence_v167 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v167 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v167 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v168 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v169 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v170 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v171 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v174 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v174 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v174 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v175 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v175 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v175 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v176 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v176 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v176 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v177 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v177 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v177 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v178 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v178 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v178 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v179 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v179 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v179 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v180 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v180 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v180 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
add_executable(Cadence_v181 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v181 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v181 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v182 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v182 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v182 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v183 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v183 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v183 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v184 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v184 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v184 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v185 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v185 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v185 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v186 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v186 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v186 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v187 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v187 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v187 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v188 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v188 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v188 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v189 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v189 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v189 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v190 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v190 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v190 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v191 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v191 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v191 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v192 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v192 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v192 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v193 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v193 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v193 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v194 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v194 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v194 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v195 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v195 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v195 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v196 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v196 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v196 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v197 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v197 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v197 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v198 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v198 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v198 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v199 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v199 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v199 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v199 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v199>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v199>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v199>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v199>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v199>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v199> ${CMAKE_SOURCE_DIR}/Cadence_v199.exe)
    add_executable(Cadence_v200 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v200 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v200 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v200 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v200>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v200>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v200>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v200>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v200>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v200> ${CMAKE_SOURCE_DIR}/Cadence_v200.exe)
    add_executable(Cadence_v201 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v201 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v201 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(v202_audit EXCLUDE_FROM_ALL tools/V202Audit.cpp)
    target_compile_options(v202_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v202_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v202 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v202 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v202 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v203 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v203 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v203 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v204 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v204 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v204 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v205 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v205 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v205 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v206 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v206 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v206 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v207 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v207 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v207 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v208 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v208 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v208 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v209 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v209 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v209 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v210 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v210 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v210 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v211 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v211 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v211 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v212 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v212 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v212 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v213 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v213 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v213 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v214 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v214 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v214 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(Cadence_v215 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v215 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v215 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v215 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v215>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v215>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v215>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v215>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v215>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v215> ${CMAKE_SOURCE_DIR}/Cadence_v215.exe)
    add_executable(Cadence_v216 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v216 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v216 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v216 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v216>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v216>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v216>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v216>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v216>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v216> ${CMAKE_SOURCE_DIR}/Cadence_v216.exe)
    add_executable(Cadence_v217 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v217 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v217 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v217 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v217>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v217>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v217>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v217>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v217>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v217> ${CMAKE_SOURCE_DIR}/Cadence_v217.exe)
    add_executable(Cadence_v218 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v218 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v218 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v218 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v218>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v218>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v218>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v218>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v218>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v218> ${CMAKE_SOURCE_DIR}/Cadence_v218.exe)
    add_executable(Cadence_v219 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v219 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v219 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v219 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v219>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v219>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v219>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v219>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v219>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v219> ${CMAKE_SOURCE_DIR}/Cadence_v219.exe)
    add_executable(Cadence_v220 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v220 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v220 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v220 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v220>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v220>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v220>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v220>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v220>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v220> ${CMAKE_SOURCE_DIR}/Cadence_v220.exe)
    add_executable(Cadence_v221 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v221 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v221 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v221 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v221>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v221>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v221>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v221>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v221>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v221> ${CMAKE_SOURCE_DIR}/Cadence_v221.exe)
    add_executable(Cadence_v222 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v222 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v222 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v222 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v222>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v222>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v222>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v222>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v222>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v222> ${CMAKE_SOURCE_DIR}/Cadence_v222.exe)
    add_executable(v203_audit EXCLUDE_FROM_ALL tools/V203Audit.cpp)
    add_executable(movement_preset_check EXCLUDE_FROM_ALL tools/MovementPresetCheck.cpp)
    add_executable(player_collision_preview_check EXCLUDE_FROM_ALL tools/PlayerCollisionPreviewCheck.cpp)
    target_compile_options(player_collision_preview_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(player_collision_preview_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(reference_controller_check EXCLUDE_FROM_ALL tools/ReferenceControllerCheck.cpp)
    target_compile_options(reference_controller_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(reference_controller_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v223 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v223 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v223 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v223 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v223>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v223>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v223>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v223>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v223>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v223> ${CMAKE_SOURCE_DIR}/Cadence_v223.exe)
    target_compile_options(movement_preset_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    add_executable(Cadence_v224 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v224 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v224 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v224 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v224>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v224>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v224>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v224>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v224>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v224> ${CMAKE_SOURCE_DIR}/Cadence_v224.exe)
    add_executable(v225_check EXCLUDE_FROM_ALL tools/V225Check.cpp)
    target_compile_options(v225_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v225_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v225 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v225 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v225 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v225 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v225>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v225>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v225>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v225>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v225>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v225> ${CMAKE_SOURCE_DIR}/Cadence_v225.exe)
    target_link_libraries(Cadence_v225 PRIVATE comdlg32 shell32 ole32)
    add_executable(v227_replay_check EXCLUDE_FROM_ALL tools/V227ReplayCheck.cpp)
    target_compile_options(v227_replay_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v227_replay_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(v226_replay_check EXCLUDE_FROM_ALL tools/V226ReplayCheck.cpp)
    target_compile_options(v226_replay_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v226_replay_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v226 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v226 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v226 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v226 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v226>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v226>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v226>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v226>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v226>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v226> ${CMAKE_SOURCE_DIR}/Cadence_v226.exe)
    add_executable(Cadence_v227 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v227 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v227 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v227 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v227>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v227>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v227>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v227>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v227>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v227> ${CMAKE_SOURCE_DIR}/Cadence_v227.exe)
    add_executable(Cadence_v228 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v228 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v228 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v228 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v228>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v228>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v228>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v228>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v228>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v228> ${CMAKE_SOURCE_DIR}/Cadence_v228.exe)
    add_executable(Cadence_v229 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v230 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v230 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v230 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v230 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v230>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v230>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v230>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v230>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v230>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v230> ${CMAKE_SOURCE_DIR}/Cadence_v230.exe)
    add_executable(Cadence_v231 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v231 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v231 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v231 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v231>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v231>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v231>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v231>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v231>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v231> ${CMAKE_SOURCE_DIR}/Cadence_v231.exe)
    add_executable(Cadence_v232 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v232 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v232 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v232 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v232>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v232>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v232>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v232>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v232>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v232> ${CMAKE_SOURCE_DIR}/Cadence_v232.exe)
    add_executable(Cadence_v233 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v233 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v233 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v233 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v233>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v233>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v233>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v233>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v233>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v233> ${CMAKE_SOURCE_DIR}/Cadence_v233.exe)
    add_executable(Cadence_v234 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v234 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v234 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v234 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v234>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v234>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v234>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v234>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v234>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v234> ${CMAKE_SOURCE_DIR}/Cadence_v234.exe)
    add_executable(Cadence_v235 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v235 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v235 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v235 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v235>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v235>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v235>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v235>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v235>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v235> ${CMAKE_SOURCE_DIR}/Cadence_v235.exe)
    add_executable(Cadence_v236 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v236 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v236 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v236 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v236>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v236>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v236>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v236>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v236>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v236> ${CMAKE_SOURCE_DIR}/Cadence_v236.exe)
    add_executable(Cadence_v237 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v237 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v237 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v237 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v237>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v237>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v237>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v237>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v237>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v237> ${CMAKE_SOURCE_DIR}/Cadence_v237.exe)
    add_executable(Cadence_v238 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v238 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v238 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v238 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v238>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v238>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v238>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v238>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v238>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v238> ${CMAKE_SOURCE_DIR}/Cadence_v238.exe)
    add_executable(Cadence_v239 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v239 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v239 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v239 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v239>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v239>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v239>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v239>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v239>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v239> ${CMAKE_SOURCE_DIR}/Cadence_v239.exe)
    add_executable(Cadence_v240 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v241 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v242 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(Cadence_v243 EXCLUDE_FROM_ALL src/app/main.cpp)
    add_executable(bot_pool_audit EXCLUDE_FROM_ALL tools/BotPoolAudit.cpp)
    add_executable(map_performance_audit EXCLUDE_FROM_ALL tools/MapPerformanceAudit.cpp)
    add_executable(full_scene_performance_audit EXCLUDE_FROM_ALL tools/FullScenePerformanceAudit.cpp)
    add_executable(cull_state_gl_audit EXCLUDE_FROM_ALL tools/CullStateGlAudit.cpp)
    add_executable(shadow_cache_audit EXCLUDE_FROM_ALL tools/ShadowCacheAudit.cpp)
    add_executable(weather_cost_audit EXCLUDE_FROM_ALL tools/WeatherCostAudit.cpp)
    add_executable(map_loading_audit EXCLUDE_FROM_ALL tools/MapLoadingAudit.cpp)
    target_link_libraries(map_loading_audit PRIVATE cast_glb_map stage_renderer cast_take glfw OpenGL::GL)
    target_link_libraries(weather_cost_audit PRIVATE stage_renderer cast_take glfw OpenGL::GL)
    add_executable(rust_weather_audit EXCLUDE_FROM_ALL tools/RustWeatherAudit.cpp)
    target_compile_options(rust_weather_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(rust_weather_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(shadow_cache_audit PRIVATE stage_renderer cast_take glfw OpenGL::GL)
    target_link_libraries(cull_state_gl_audit PRIVATE stage_renderer cast_take glfw OpenGL::GL)
    add_executable(hbao_radius_audit EXCLUDE_FROM_ALL tools/HbaoRadiusAudit.cpp)
    target_link_libraries(hbao_radius_audit PRIVATE stage_renderer glfw OpenGL::GL)
    add_executable(decal_lighting_audit EXCLUDE_FROM_ALL tools/DecalLightingAudit.cpp)
    target_link_libraries(decal_lighting_audit PRIVATE stage_renderer cast_take glfw OpenGL::GL)
    add_executable(catalog_compatibility_audit EXCLUDE_FROM_ALL tools/CatalogCompatibilityAudit.cpp)
    target_link_libraries(catalog_compatibility_audit PRIVATE cast_assets)
    target_compile_options(full_scene_performance_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(full_scene_performance_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(body_preparation_audit EXCLUDE_FROM_ALL tools/BodyPreparationAudit.cpp)
    target_compile_options(body_preparation_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(body_preparation_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(map_performance_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(map_performance_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(bot_pool_audit PRIVATE cast_core cast_scene)
    add_executable(Cadence_v266 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v266 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v266 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v266 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v266>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v266>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v266>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v266>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v266>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v266> ${CMAKE_SOURCE_DIR}/Cadence_v266.exe)
    add_executable(Cadence_v319 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v319 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v319 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v319 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v319>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v319>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v319>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v319>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v319>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v319> ${CMAKE_SOURCE_DIR}/Cadence_v319.exe)
    add_executable(Cadence_v320 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v320 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v320 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v320 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v320>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v320>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v320>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v320>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v320>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v320> ${CMAKE_SOURCE_DIR}/Cadence_v320.exe)
    add_executable(Cadence_v321 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v321 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v321 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v321 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v321>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v321>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v321>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v321>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v321>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v321> ${CMAKE_SOURCE_DIR}/Cadence_v321.exe)
    add_executable(Cadence_v322 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v322 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v322 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v322 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v322>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v322>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v322>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v322>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v322>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v322> ${CMAKE_SOURCE_DIR}/Cadence_v322.exe)
    add_executable(Cadence_v323 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v323 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v323 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v323 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v323>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v323>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v323>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v323>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v323>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v323> ${CMAKE_SOURCE_DIR}/Cadence_v323.exe)
    add_executable(Cadence_v361 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v361 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v361 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v362 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v362 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v362 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v363 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v363 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v363 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v367 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v367 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v367 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v367 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v367>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v367>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v367>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v367>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v367>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v367> ${CMAKE_SOURCE_DIR}/Cadence_v367.exe)
    add_executable(Cadence_v368 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v368 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v368 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v368 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v368>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v368>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v368>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v368>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v368>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v368> ${CMAKE_SOURCE_DIR}/Cadence_v368.exe)
    add_executable(Cadence_v369 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v369 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v369 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v369 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v369>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v369>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v369>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v369>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v369>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v369> ${CMAKE_SOURCE_DIR}/Cadence_v369.exe)
    add_executable(Cadence_v370 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v370 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v370 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v370 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v370>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v370>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v370>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v370>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v370>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v370> ${CMAKE_SOURCE_DIR}/Cadence_v370.exe)
    add_executable(Cadence_v371 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v371 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v371 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v371 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v371>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v371>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v371>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v371>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v371>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v371> ${CMAKE_SOURCE_DIR}/Cadence_v371.exe)
    add_executable(Cadence_v373 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v373 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v373 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v374 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v374 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v374 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v375 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v375 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v375 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v376 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v376 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v376 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v377 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v377 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v377 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v378 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v378 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v378 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v388 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v388 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v388 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v388 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v388> ${CMAKE_SOURCE_DIR}/Cadence_v388.exe)
    add_executable(Cadence_v389 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v389 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v389 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v389 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v389> ${CMAKE_SOURCE_DIR}/Cadence_v389.exe)
    add_executable(Cadence_v390 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v390 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v390 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v390 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v390> ${CMAKE_SOURCE_DIR}/Cadence_v390.exe)
    add_executable(Cadence_v391 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v391 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v391 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v391 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v391> ${CMAKE_SOURCE_DIR}/Cadence_v391.exe)
    add_executable(Cadence_v392 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v392 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v392 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v392 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v392> ${CMAKE_SOURCE_DIR}/Cadence_v392.exe)
    add_executable(Cadence_v393 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v393 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v393 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v393 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v393> ${CMAKE_SOURCE_DIR}/Cadence_v393.exe)
    add_executable(Cadence_v394 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v394 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v394 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v394 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v394> ${CMAKE_SOURCE_DIR}/Cadence_v394.exe)
    add_executable(Cadence_v395 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v395 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v395 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v395 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v395> ${CMAKE_SOURCE_DIR}/Cadence_v395.exe)
    add_executable(Cadence_v396 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v396 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v396 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v396 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v396> ${CMAKE_SOURCE_DIR}/Cadence_v396.exe)
    add_executable(Cadence_v397 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v397 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v397 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v397 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v397> ${CMAKE_SOURCE_DIR}/Cadence_v397.exe)
    add_executable(Cadence_v398 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v398 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v398 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v398 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v398> ${CMAKE_SOURCE_DIR}/Cadence_v398.exe)
    add_executable(Cadence_v399 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v399 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v399 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v399 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v399> ${CMAKE_SOURCE_DIR}/Cadence_v399.exe)
    add_executable(Cadence_v400 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v400 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v400 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v400 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v400> ${CMAKE_SOURCE_DIR}/Cadence_v400.exe)
    add_executable(Cadence_v401 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v401 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v401 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v401 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v401> ${CMAKE_SOURCE_DIR}/Cadence_v401.exe)
    add_executable(Cadence_v402 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v402 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v402 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v402 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v402> ${CMAKE_SOURCE_DIR}/Cadence_v402.exe)
    add_executable(Cadence_v403 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v403 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v403 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v403 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v403> ${CMAKE_SOURCE_DIR}/Cadence_v403.exe)
    add_executable(Cadence_v404 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v404 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v404 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v404 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v404> ${CMAKE_SOURCE_DIR}/Cadence_v404.exe)
    add_executable(Cadence_v405 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v405 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v405 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v405 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v405> ${CMAKE_SOURCE_DIR}/Cadence_v405.exe)
    add_executable(Cadence_v406 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v406 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v406 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v406 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v406> ${CMAKE_SOURCE_DIR}/Cadence_v406.exe)
    add_executable(Cadence_v407 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v407 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v407 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v407 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v407> ${CMAKE_SOURCE_DIR}/Cadence_v407.exe)
    add_executable(Cadence_v408 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v408 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v408 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v408 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v408> ${CMAKE_SOURCE_DIR}/Cadence_v408.exe)
    add_executable(Cadence_v409 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v409 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v409 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v409 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v409> ${CMAKE_SOURCE_DIR}/Cadence_v409.exe)
    add_executable(Cadence_v410 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v410 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v410 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v410 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v410> ${CMAKE_SOURCE_DIR}/Cadence_v410.exe)
    add_executable(Cadence_v411 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v411 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v411 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v411 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v411> ${CMAKE_SOURCE_DIR}/Cadence_v411.exe)
    add_executable(Cadence_v412 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v412 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v412 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v412 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v412> ${CMAKE_SOURCE_DIR}/Cadence_v412.exe)
    add_executable(Cadence_v413 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v413 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v413 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v413 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v413> ${CMAKE_SOURCE_DIR}/Cadence_v413.exe)
    add_executable(Cadence_v414 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v414 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v414 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v414 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v414> ${CMAKE_SOURCE_DIR}/Cadence_v414.exe)
    add_executable(Cadence_v415 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v415 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v415 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v415 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v415> ${CMAKE_SOURCE_DIR}/Cadence_v415.exe)
    add_executable(Cadence_v416 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v416 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v416 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v416 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v416> ${CMAKE_SOURCE_DIR}/Cadence_v416.exe)
    add_executable(Cadence_v417 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v417 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v417 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v417 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v417> ${CMAKE_SOURCE_DIR}/Cadence_v417.exe)
    add_executable(Cadence_v418 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v418 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v418 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v418 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v418> ${CMAKE_SOURCE_DIR}/Cadence_v418.exe)
    add_executable(Cadence_v419 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v419 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v419 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v419 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v419> ${CMAKE_SOURCE_DIR}/Cadence_v419.exe)
    add_executable(Cadence_v420 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v420 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v420 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v420 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v420> ${CMAKE_SOURCE_DIR}/Cadence_v420.exe)
    add_executable(Cadence_v421 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v421 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v421 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v421 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v421> ${CMAKE_SOURCE_DIR}/Cadence_v421.exe)
    add_executable(Cadence_v422 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v422 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v422 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v422 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v422> ${CMAKE_SOURCE_DIR}/Cadence_v422.exe)
    add_executable(Cadence_v423 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v423 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v423 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v423 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v423> ${CMAKE_SOURCE_DIR}/Cadence_v423.exe)
    add_executable(Cadence_v424 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v424 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v424 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v424 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v424> ${CMAKE_SOURCE_DIR}/Cadence_v424.exe)
    add_executable(Cadence_v425 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v425 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v425 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v425 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v425> ${CMAKE_SOURCE_DIR}/Cadence_v425.exe)
    add_executable(Cadence_v426 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v426 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v426 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v426 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v426> ${CMAKE_SOURCE_DIR}/Cadence_v426.exe)
    add_executable(Cadence_v427 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v427 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v427 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v427 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v427> ${CMAKE_SOURCE_DIR}/Cadence_v427.exe)
    add_executable(Cadence_v428 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v428 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v428 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v428 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v428> ${CMAKE_SOURCE_DIR}/Cadence_v428.exe)
    add_executable(Cadence_v429 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v429 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v429 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v429 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v429> ${CMAKE_SOURCE_DIR}/Cadence_v429.exe)
    add_executable(Cadence_v430 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v430 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v430 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v430 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v430> ${CMAKE_SOURCE_DIR}/Cadence_v430.exe)
    add_executable(Cadence_v431 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v431 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v431 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v431 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v431> ${CMAKE_SOURCE_DIR}/Cadence_v431.exe)
    add_executable(Cadence_v432 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v432 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v432 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v432 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v432> ${CMAKE_SOURCE_DIR}/Cadence_v432.exe)
    add_executable(Cadence_v433 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v433 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v433 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v433 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v433> ${CMAKE_SOURCE_DIR}/Cadence_v433.exe)
    add_executable(Cadence_v434 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v434 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v434 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v434 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v434> ${CMAKE_SOURCE_DIR}/Cadence_v434.exe)
    add_executable(Cadence_v435 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v435 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v435 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v435 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v435> ${CMAKE_SOURCE_DIR}/Cadence_v435.exe)
    add_executable(Cadence_v436 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v436 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v436 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v436 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v436> ${CMAKE_SOURCE_DIR}/Cadence_v436.exe)
    add_executable(Cadence_v437 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v437 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v437 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v437 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v437> ${CMAKE_SOURCE_DIR}/Cadence_v437.exe)
    add_executable(Cadence_v438 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v438 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v438 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v438 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v438> ${CMAKE_SOURCE_DIR}/Cadence_v438.exe)
    add_executable(Cadence_v439 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v439 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v439 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v439 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v439> ${CMAKE_SOURCE_DIR}/Cadence_v439.exe)
    add_executable(Cadence_v440 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v440 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v440 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v440 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v440> ${CMAKE_SOURCE_DIR}/Cadence_v440.exe)
    add_executable(Cadence_v441 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v441 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v441 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v441 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v441> ${CMAKE_SOURCE_DIR}/Cadence_v441.exe)
    add_executable(Cadence_v442 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v442 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v442 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v442 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v442> ${CMAKE_SOURCE_DIR}/Cadence_v442.exe)
    add_executable(Cadence_v443 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v443 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v443 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v443 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v443> ${CMAKE_SOURCE_DIR}/Cadence_v443.exe)
    add_executable(Cadence_v446 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v446 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v446 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v446 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v446> ${CMAKE_SOURCE_DIR}/Cadence_v446.exe)
    add_executable(Cadence_v447 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v447 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v447 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v447 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v447> ${CMAKE_SOURCE_DIR}/Cadence_v447.exe)
    add_executable(Cadence_v448 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v448 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v448 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v448 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v448> ${CMAKE_SOURCE_DIR}/Cadence_v448.exe)
    add_executable(Cadence_v449 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v449 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v449 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v449 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v449> ${CMAKE_SOURCE_DIR}/Cadence_v449.exe)
    add_executable(Cadence_v450 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v450 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v450 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v450 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v450> ${CMAKE_SOURCE_DIR}/Cadence_v450.exe)
    add_executable(Cadence_v451 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v451 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v451 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v451 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v451> ${CMAKE_SOURCE_DIR}/Cadence_v451.exe)
    add_executable(Cadence_v452 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v452 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v452 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v452 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v452> ${CMAKE_SOURCE_DIR}/Cadence_v452.exe)
    add_executable(Cadence_v453 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v453 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v453 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v453 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v453> ${CMAKE_SOURCE_DIR}/Cadence_v453.exe)
    add_executable(Cadence_v454 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v454 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v454 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v454 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v454> ${CMAKE_SOURCE_DIR}/Cadence_v454.exe)
    add_executable(Cadence_v455 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v455 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v455 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v455 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v455> ${CMAKE_SOURCE_DIR}/Cadence_v455.exe)
    add_executable(Cadence_v456 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v456 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v456 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v456 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v456> ${CMAKE_SOURCE_DIR}/Cadence_v456.exe)
    add_executable(Cadence_v457 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v457 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v457 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v457 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v457> ${CMAKE_SOURCE_DIR}/Cadence_v457.exe)
    add_executable(Cadence_v458 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v458 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v458 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v458 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v458> ${CMAKE_SOURCE_DIR}/Cadence_v458.exe)
    add_executable(melee_mount_probe EXCLUDE_FROM_ALL tools/MeleeMountProbe.cpp)
    add_executable(mw3_material_probe EXCLUDE_FROM_ALL tools/Mw3MaterialProbe.cpp)
    target_link_libraries(mw3_material_probe PRIVATE cast_scene stage_renderer cast_take glfw OpenGL::GL)
    target_link_libraries(melee_mount_probe PRIVATE cast_scene cast_assets)
    add_executable(Cadence_v465 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v465 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v465 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v465 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v465> ${CMAKE_SOURCE_DIR}/Cadence_v465.exe)
    add_executable(Cadence_v466 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v466 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v466 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v466 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v466> ${CMAKE_SOURCE_DIR}/Cadence_v466.exe)
    add_executable(Cadence_v467 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v467 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v467 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v467 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v467> ${CMAKE_SOURCE_DIR}/Cadence_v467.exe)
    add_executable(Cadence_v468 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v468 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v468 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v468 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v468> ${CMAKE_SOURCE_DIR}/Cadence_v468.exe)
    add_executable(Cadence_v469 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v469 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v469 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v469 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v469> ${CMAKE_SOURCE_DIR}/Cadence_v469.exe)
    add_executable(Cadence_v470 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v470 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v470 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v470 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v470> ${CMAKE_SOURCE_DIR}/Cadence_v470.exe)
    add_executable(Cadence_v471 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v471 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v471 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v471 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v471> ${CMAKE_SOURCE_DIR}/Cadence_v471.exe)
    add_executable(Cadence_v472 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v472 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v472 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v472 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v472> ${CMAKE_SOURCE_DIR}/Cadence_v472.exe)
    add_executable(Cadence_v473 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v473 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v473 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v473 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v473> ${CMAKE_SOURCE_DIR}/Cadence_v473.exe)
    add_executable(Cadence_v474 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v474 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v474 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v474 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v474> ${CMAKE_SOURCE_DIR}/Cadence_v474.exe)
    add_executable(map_spawn_audit EXCLUDE_FROM_ALL tools/MapSpawnAudit.cpp)
    target_link_libraries(map_spawn_audit PRIVATE cast_glb_map)
    add_executable(source_muzzle_audit EXCLUDE_FROM_ALL tools/SourceMuzzleAudit.cpp)
    target_link_libraries(source_muzzle_audit PRIVATE cast_scene)
    add_executable(shader_disassemble EXCLUDE_FROM_ALL tools/ShaderDisassemble.cpp)
    target_link_libraries(shader_disassemble PRIVATE d3dcompiler)
    add_executable(metal_shader_audit EXCLUDE_FROM_ALL tools/MetalShaderAudit.cpp)
    target_link_libraries(metal_shader_audit PRIVATE stage_renderer cast_scene cast_take OpenGL::GL)
    add_executable(score_preview_audit EXCLUDE_FROM_ALL tools/ScorePreviewAudit.cpp)
    target_link_libraries(score_preview_audit PRIVATE stage_renderer cast_scene cast_take imgui_backend OpenGL::GL)
    add_executable(foreign_bo_hands_audit EXCLUDE_FROM_ALL tools/ForeignBoHandsAudit.cpp)
    target_compile_options(foreign_bo_hands_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(foreign_bo_hands_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v500 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v500 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v500 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)

    add_executable(Cadence_v501 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v501 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v501 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)

    target_link_libraries(cast_glb_map PUBLIC box3d::box3d)
    add_executable(Cadence_v502 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v502 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v502 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)

    add_executable(Cadence_v503 src/app/main.cpp)
    target_compile_options(Cadence_v503 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v503 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v503 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v503> ${CMAKE_SOURCE_DIR}/Cadence_v503.exe)
    add_executable(Cadence_v499 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v499 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v499 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v499 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v499> ${CMAKE_SOURCE_DIR}/Cadence_v499.exe)
    add_executable(Cadence_v498 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v498 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v498 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v498 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v498> ${CMAKE_SOURCE_DIR}/Cadence_v498.exe)
    add_executable(Cadence_v497 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v497 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v497 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v497 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v497> ${CMAKE_SOURCE_DIR}/Cadence_v497.exe)
    add_executable(Cadence_v496 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v496 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v496 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v496 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v496> ${CMAKE_SOURCE_DIR}/Cadence_v496.exe)
    add_executable(Cadence_v495 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v495 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v495 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v495 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v495> ${CMAKE_SOURCE_DIR}/Cadence_v495.exe)
    add_executable(Cadence_v494 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v494 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v494 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v494 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v494> ${CMAKE_SOURCE_DIR}/Cadence_v494.exe)
    add_executable(Cadence_v493 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v493 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v493 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v493 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v493> ${CMAKE_SOURCE_DIR}/Cadence_v493.exe)
    add_executable(Cadence_v492 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v492 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v492 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v492 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v492> ${CMAKE_SOURCE_DIR}/Cadence_v492.exe)
    add_executable(Cadence_v491 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v491 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v491 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v491 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v491> ${CMAKE_SOURCE_DIR}/Cadence_v491.exe)
    add_executable(Cadence_v490 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v490 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v490 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v490 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v490> ${CMAKE_SOURCE_DIR}/Cadence_v490.exe)
    add_executable(Cadence_v489 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v489 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v489 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v489 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v489> ${CMAKE_SOURCE_DIR}/Cadence_v489.exe)
    add_executable(Cadence_v488 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v488 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v488 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v488 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v488> ${CMAKE_SOURCE_DIR}/Cadence_v488.exe)
    add_executable(weapon488_audit EXCLUDE_FROM_ALL tools/Weapon488Audit.cpp)
    target_compile_options(weapon488_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(weapon488_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v487 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v487 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v487 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v487 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v487> ${CMAKE_SOURCE_DIR}/Cadence_v487.exe)
    add_executable(Cadence_v486 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v486 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v486 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v486 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v486> ${CMAKE_SOURCE_DIR}/Cadence_v486.exe)
    add_executable(Cadence_v485 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v485 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v485 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v485 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v485> ${CMAKE_SOURCE_DIR}/Cadence_v485.exe)
    add_executable(Cadence_v484 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v484 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v484 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v484 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v484> ${CMAKE_SOURCE_DIR}/Cadence_v484.exe)
    add_executable(Cadence_v483 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v483 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v483 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v483 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v483> ${CMAKE_SOURCE_DIR}/Cadence_v483.exe)
    add_executable(Cadence_v482 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v482 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v482 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v482 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v482> ${CMAKE_SOURCE_DIR}/Cadence_v482.exe)
    add_executable(Cadence_v481 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v481 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v481 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v481 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v481> ${CMAKE_SOURCE_DIR}/Cadence_v481.exe)
    add_executable(Cadence_v480 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v480 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v480 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v480 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v480> ${CMAKE_SOURCE_DIR}/Cadence_v480.exe)
    add_executable(Cadence_v479 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v479 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v479 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v479 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v479> ${CMAKE_SOURCE_DIR}/Cadence_v479.exe)
    add_executable(Cadence_v478 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v478 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v478 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v478 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v478> ${CMAKE_SOURCE_DIR}/Cadence_v478.exe)
    add_executable(Cadence_v477 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v477 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v477 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v477 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v477> ${CMAKE_SOURCE_DIR}/Cadence_v477.exe)
    add_executable(Cadence_v476 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v476 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v476 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v476 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v476> ${CMAKE_SOURCE_DIR}/Cadence_v476.exe)
    add_executable(Cadence_v475 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v475 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v475 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon stage_renderer cast_glb_map imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v475 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v475> ${CMAKE_SOURCE_DIR}/Cadence_v475.exe)
    add_executable(Cadence_v464 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v464 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v464 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v464 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v464> ${CMAKE_SOURCE_DIR}/Cadence_v464.exe)
    add_executable(Cadence_v463 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v463 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v463 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v463 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v463> ${CMAKE_SOURCE_DIR}/Cadence_v463.exe)
    add_executable(Cadence_v462 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v462 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v462 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v462 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v462> ${CMAKE_SOURCE_DIR}/Cadence_v462.exe)
    add_executable(Cadence_v461 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v461 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v461 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v461 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v461> ${CMAKE_SOURCE_DIR}/Cadence_v461.exe)
    add_executable(Cadence_v460 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v460 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v460 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v460 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v460> ${CMAKE_SOURCE_DIR}/Cadence_v460.exe)
    add_executable(Cadence_v459 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v459 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v459 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v459 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v459> ${CMAKE_SOURCE_DIR}/Cadence_v459.exe)
    add_executable(Cadence_v445 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v445 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v445 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v445 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v445> ${CMAKE_SOURCE_DIR}/Cadence_v445.exe)
    add_executable(Cadence_v444 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v444 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v444 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v444 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v444> ${CMAKE_SOURCE_DIR}/Cadence_v444.exe)
    add_executable(Cadence_v387 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v387 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v387 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v387 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v387> ${CMAKE_SOURCE_DIR}/Cadence_v387.exe)
    add_executable(Cadence_v386 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v386 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v386 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v386 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v386> ${CMAKE_SOURCE_DIR}/Cadence_v386.exe)
    add_executable(Cadence_v385 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v385 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v385 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v385 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v385> ${CMAKE_SOURCE_DIR}/Cadence_v385.exe)
    add_executable(Cadence_v384 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v384 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v384 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v384 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v384> ${CMAKE_SOURCE_DIR}/Cadence_v384.exe)
    add_executable(Cadence_v383 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v383 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v383 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v383 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v383> ${CMAKE_SOURCE_DIR}/Cadence_v383.exe)
    add_executable(Cadence_v382 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v382 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v382 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v382 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v382> ${CMAKE_SOURCE_DIR}/Cadence_v382.exe)
    add_executable(Cadence_v381 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v381 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v381 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v381 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v381> ${CMAKE_SOURCE_DIR}/Cadence_v381.exe)
    add_executable(Cadence_v380 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v380 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v380 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v380 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v380>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v380>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v380>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v380>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v380>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v380> ${CMAKE_SOURCE_DIR}/Cadence_v380.exe)
    add_executable(Cadence_v379 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v379 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v379 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v379 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v379>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v379>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v379>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v379>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v379>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v379> ${CMAKE_SOURCE_DIR}/Cadence_v379.exe)
    add_executable(Cadence_v372 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v372 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v372 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v372 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v372>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v372>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v372>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v372>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v372>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v372> ${CMAKE_SOURCE_DIR}/Cadence_v372.exe)
    add_executable(Cadence_v366 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v366 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v366 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v366 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v366>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v366>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v366>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v366>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v366>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v366> ${CMAKE_SOURCE_DIR}/Cadence_v366.exe)
    add_executable(Cadence_v365 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v365 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v365 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v365 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v365>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v365>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v365>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v365>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v365>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v365> ${CMAKE_SOURCE_DIR}/Cadence_v365.exe)
    add_executable(Cadence_v364 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v364 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v364 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v364 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v364>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v364>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v364>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v364>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v364>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v364> ${CMAKE_SOURCE_DIR}/Cadence_v364.exe)
    add_custom_command(TARGET Cadence_v363 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v363>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v363>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v363>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v363>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v363>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v363> ${CMAKE_SOURCE_DIR}/Cadence_v363.exe)
    add_custom_command(TARGET Cadence_v362 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v362>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v362>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v362>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v362>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v362>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v362> ${CMAKE_SOURCE_DIR}/Cadence_v362.exe)
    add_custom_command(TARGET Cadence_v361 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v361>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v361>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v361>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v361>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v361>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v361> ${CMAKE_SOURCE_DIR}/Cadence_v361.exe)
    add_executable(Cadence_v360 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v360 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v360 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v360 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v360>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v360>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v360>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v360>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v360>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v360> ${CMAKE_SOURCE_DIR}/Cadence_v360.exe)
    add_executable(Cadence_v359 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v359 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v359 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v359 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v359>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v359>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v359>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v359>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v359>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v359> ${CMAKE_SOURCE_DIR}/Cadence_v359.exe)
    add_executable(Cadence_v358 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v358 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v358 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v358 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v358>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v358>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v358>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v358>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v358>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v358> ${CMAKE_SOURCE_DIR}/Cadence_v358.exe)
    add_executable(Cadence_v357 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v357 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v357 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v357 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v357>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v357>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v357>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v357>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v357>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v357> ${CMAKE_SOURCE_DIR}/Cadence_v357.exe)
    add_executable(Cadence_v356 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v356 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v356 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v356 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v356>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v356>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v356>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v356>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v356>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v356> ${CMAKE_SOURCE_DIR}/Cadence_v356.exe)
    add_executable(Cadence_v355 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v355 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v355 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v355 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v355>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v355>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v355>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v355>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v355>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v355> ${CMAKE_SOURCE_DIR}/Cadence_v355.exe)
    add_executable(Cadence_v354 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v354 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v354 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v354 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v354>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v354>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v354>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v354>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v354>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v354> ${CMAKE_SOURCE_DIR}/Cadence_v354.exe)
    add_executable(Cadence_v353 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v353 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v353 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v353 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v353>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v353>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v353>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v353>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v353>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v353> ${CMAKE_SOURCE_DIR}/Cadence_v353.exe)
    add_executable(Cadence_v352 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v352 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v352 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v352 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v352>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v352>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v352>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v352>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v352>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v352> ${CMAKE_SOURCE_DIR}/Cadence_v352.exe)
    add_executable(Cadence_v351 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v351 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v351 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v351 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v351>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v351>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v351>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v351>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v351>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v351> ${CMAKE_SOURCE_DIR}/Cadence_v351.exe)
    add_executable(Cadence_v350 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v350 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v350 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v350 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v350>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v350>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v350>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v350>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v350>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v350> ${CMAKE_SOURCE_DIR}/Cadence_v350.exe)
    add_executable(Cadence_v349 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v349 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v349 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v349 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v349>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v349>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v349>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v349>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v349>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v349> ${CMAKE_SOURCE_DIR}/Cadence_v349.exe)
    add_executable(Cadence_v348 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v348 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v348 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v348 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v348>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v348>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v348>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v348>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v348>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v348> ${CMAKE_SOURCE_DIR}/Cadence_v348.exe)
    add_executable(Cadence_v347 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v347 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v347 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v347 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v347>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v347>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v347>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v347>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v347>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v347> ${CMAKE_SOURCE_DIR}/Cadence_v347.exe)
    add_executable(Cadence_v346 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v346 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v346 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v346 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v346>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v346>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v346>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v346>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v346>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v346> ${CMAKE_SOURCE_DIR}/Cadence_v346.exe)
    add_executable(Cadence_v345 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v345 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v345 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v345 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v345>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v345>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v345>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v345>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v345>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v345> ${CMAKE_SOURCE_DIR}/Cadence_v345.exe)
    add_executable(Cadence_v344 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v344 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v344 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v344 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v344>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v344>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v344>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v344>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v344>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v344> ${CMAKE_SOURCE_DIR}/Cadence_v344.exe)
    add_executable(Cadence_v343 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v343 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v343 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v343 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v343>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v343>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v343>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v343>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v343>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v343> ${CMAKE_SOURCE_DIR}/Cadence_v343.exe)
    add_executable(Cadence_v342 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v342 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v342 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v342 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v342>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v342>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v342>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v342>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v342>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v342> ${CMAKE_SOURCE_DIR}/Cadence_v342.exe)
    add_executable(Cadence_v341 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v341 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v341 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v341 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v341>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v341>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v341>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v341>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v341>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v341> ${CMAKE_SOURCE_DIR}/Cadence_v341.exe)
    add_executable(Cadence_v340 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v340 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v340 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v340 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v340>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v340>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v340>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v340>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v340>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v340> ${CMAKE_SOURCE_DIR}/Cadence_v340.exe)
    add_executable(Cadence_v339 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v339 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v339 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v339 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v339>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v339>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v339>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v339>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v339>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v339> ${CMAKE_SOURCE_DIR}/Cadence_v339.exe)
    add_executable(Cadence_v338 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v338 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v338 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v338 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v338>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v338>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v338>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v338>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v338>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v338> ${CMAKE_SOURCE_DIR}/Cadence_v338.exe)
    add_executable(Cadence_v337 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v337 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v337 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v337 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v337>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v337>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v337>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v337>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v337>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v337> ${CMAKE_SOURCE_DIR}/Cadence_v337.exe)
    add_executable(Cadence_v336 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v336 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v336 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v336 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v336>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v336>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v336>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v336>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v336>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v336> ${CMAKE_SOURCE_DIR}/Cadence_v336.exe)
    add_executable(Cadence_v335 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v335 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v335 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v335 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v335>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v335>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v335>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v335>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v335>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v335> ${CMAKE_SOURCE_DIR}/Cadence_v335.exe)
    add_executable(Cadence_v334 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v334 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v334 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v334 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v334>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v334>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v334>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v334>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v334>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v334> ${CMAKE_SOURCE_DIR}/Cadence_v334.exe)
    add_executable(Cadence_v333 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v333 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v333 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v333 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v333>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v333>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v333>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v333>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v333>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v333> ${CMAKE_SOURCE_DIR}/Cadence_v333.exe)
    add_executable(Cadence_v332 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v332 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v332 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v332 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v332>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v332>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v332>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v332>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v332>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v332> ${CMAKE_SOURCE_DIR}/Cadence_v332.exe)
    add_executable(Cadence_v331 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v331 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v331 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v331 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v331>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v331>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v331>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v331>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v331>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v331> ${CMAKE_SOURCE_DIR}/Cadence_v331.exe)
    add_executable(Cadence_v330 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v330 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v330 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v330 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v330>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v330>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v330>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v330>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v330>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v330> ${CMAKE_SOURCE_DIR}/Cadence_v330.exe)
    add_executable(Cadence_v329 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v329 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v329 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v329 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v329>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v329>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v329>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v329>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v329>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v329> ${CMAKE_SOURCE_DIR}/Cadence_v329.exe)
    add_executable(Cadence_v328 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v328 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v328 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v328 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v328>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v328>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v328>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v328>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v328>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v328> ${CMAKE_SOURCE_DIR}/Cadence_v328.exe)
    add_executable(Cadence_v327 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v327 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v327 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v327 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v327>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v327>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v327>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v327>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v327>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v327> ${CMAKE_SOURCE_DIR}/Cadence_v327.exe)
    add_executable(Cadence_v326 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v326 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v326 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v326 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v326>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v326>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v326>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v326>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v326>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v326> ${CMAKE_SOURCE_DIR}/Cadence_v326.exe)
    add_executable(Cadence_v325 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v325 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v325 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v325 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v325>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v325>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v325>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v325>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v325>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v325> ${CMAKE_SOURCE_DIR}/Cadence_v325.exe)
    add_executable(Cadence_v324 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v324 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v324 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v324 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v324>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v324>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v324>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v324>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v324>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v324> ${CMAKE_SOURCE_DIR}/Cadence_v324.exe)
    add_executable(Cadence_v318 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v318 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v318 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v318 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v318>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v318>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v318>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v318>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v318>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v318> ${CMAKE_SOURCE_DIR}/Cadence_v318.exe)
    add_executable(Cadence_v317 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v317 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v317 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v317 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v317>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v317>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v317>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v317>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v317>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v317> ${CMAKE_SOURCE_DIR}/Cadence_v317.exe)
    add_executable(Cadence_v316 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v316 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v316 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v316 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v316>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v316>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v316>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v316>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v316>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v316> ${CMAKE_SOURCE_DIR}/Cadence_v316.exe)
    add_executable(Cadence_v315 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v315 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v315 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v315 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v315>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v315>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v315>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v315>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v315>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v315> ${CMAKE_SOURCE_DIR}/Cadence_v315.exe)
    add_executable(Cadence_v314 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v314 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v314 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v314 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v314>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v314>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v314>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v314>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v314>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v314> ${CMAKE_SOURCE_DIR}/Cadence_v314.exe)
    add_executable(Cadence_v313 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v313 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v313 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v313 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v313>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v313>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v313>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v313>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v313>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v313> ${CMAKE_SOURCE_DIR}/Cadence_v313.exe)
    add_executable(Cadence_v312 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v312 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v312 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v312 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v312>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v312>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v312>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v312>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v312>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v312> ${CMAKE_SOURCE_DIR}/Cadence_v312.exe)
    add_executable(Cadence_v311 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v311 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v311 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v311 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v311>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v311>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v311>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v311>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v311>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v311> ${CMAKE_SOURCE_DIR}/Cadence_v311.exe)
    add_executable(Cadence_v310 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v310 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v310 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v310 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v310>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v310>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v310>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v310>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v310>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v310> ${CMAKE_SOURCE_DIR}/Cadence_v310.exe)
    add_executable(Cadence_v309 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v309 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v309 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v309 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v309>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v309>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v309>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v309>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v309>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v309> ${CMAKE_SOURCE_DIR}/Cadence_v309.exe)
    add_executable(Cadence_v308 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v308 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v308 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v308 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v308>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v308>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v308>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v308>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v308>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v308> ${CMAKE_SOURCE_DIR}/Cadence_v308.exe)
    add_executable(Cadence_v307 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v307 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v307 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v307 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v307>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v307>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v307>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v307>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v307>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v307> ${CMAKE_SOURCE_DIR}/Cadence_v307.exe)
    add_executable(Cadence_v306 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v306 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v306 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v306 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v306>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v306>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v306>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v306>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v306>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v306> ${CMAKE_SOURCE_DIR}/Cadence_v306.exe)
    add_executable(Cadence_v305 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v305 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v305 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v305 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v305>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v305>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v305>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v305>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v305>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v305> ${CMAKE_SOURCE_DIR}/Cadence_v305.exe)
    add_executable(Cadence_v304 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v304 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v304 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v304 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v304>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v304>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v304>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v304>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v304>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v304> ${CMAKE_SOURCE_DIR}/Cadence_v304.exe)
    add_executable(Cadence_v303 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v303 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v303 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v303 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v303>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v303>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v303>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v303>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v303>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v303> ${CMAKE_SOURCE_DIR}/Cadence_v303.exe)
    add_executable(Cadence_v302 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v302 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v302 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v302 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v302>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v302>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v302>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v302>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v302>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v302> ${CMAKE_SOURCE_DIR}/Cadence_v302.exe)
    add_executable(Cadence_v301 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v301 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v301 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v301 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v301>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v301>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v301>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v301>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v301>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v301> ${CMAKE_SOURCE_DIR}/Cadence_v301.exe)
    add_executable(Cadence_v300 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v300 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v300 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v300 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v300>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v300>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v300>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v300>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v300>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v300> ${CMAKE_SOURCE_DIR}/Cadence_v300.exe)
    add_executable(Cadence_v299 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v299 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v299 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v299 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v299>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v299>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v299>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v299>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v299>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v299> ${CMAKE_SOURCE_DIR}/Cadence_v299.exe)
    add_executable(Cadence_v298 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v298 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v298 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v298 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v298>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v298>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v298>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v298>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v298>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v298> ${CMAKE_SOURCE_DIR}/Cadence_v298.exe)
    add_executable(Cadence_v297 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v297 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v297 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v297 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v297>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v297>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v297>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v297>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v297>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v297> ${CMAKE_SOURCE_DIR}/Cadence_v297.exe)
    add_executable(Cadence_v296 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v296 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v296 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v296 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v296>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v296>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v296>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v296>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v296>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v296> ${CMAKE_SOURCE_DIR}/Cadence_v296.exe)
    add_executable(Cadence_v295 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v295 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v295 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v295 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v295>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v295>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v295>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v295>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v295>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v295> ${CMAKE_SOURCE_DIR}/Cadence_v295.exe)
    add_executable(Cadence_v294 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v294 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v294 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v294 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v294>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v294>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v294>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v294>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v294>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v294> ${CMAKE_SOURCE_DIR}/Cadence_v294.exe)
    add_executable(Cadence_v293 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v293 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v293 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v293 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v293>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v293>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v293>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v293>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v293>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v293> ${CMAKE_SOURCE_DIR}/Cadence_v293.exe)
    add_executable(Cadence_v292 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v292 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v292 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v292 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v292>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v292>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v292>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v292>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v292>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v292> ${CMAKE_SOURCE_DIR}/Cadence_v292.exe)
    add_executable(Cadence_v291 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v291 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v291 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v291 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v291>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v291>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v291>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v291>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v291>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v291> ${CMAKE_SOURCE_DIR}/Cadence_v291.exe)
    add_executable(Cadence_v290 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v290 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v290 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v290 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v290>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v290>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v290>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v290>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v290>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v290> ${CMAKE_SOURCE_DIR}/Cadence_v290.exe)
    add_executable(Cadence_v289 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v289 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v289 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v289 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v289>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v289>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v289>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v289>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v289>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v289> ${CMAKE_SOURCE_DIR}/Cadence_v289.exe)
    add_executable(Cadence_v288 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v288 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v288 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v288 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v288>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v288>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v288>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v288>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v288>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v288> ${CMAKE_SOURCE_DIR}/Cadence_v288.exe)
    add_executable(Cadence_v287 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v287 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v287 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v287 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v287>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v287>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v287>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v287>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v287>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v287> ${CMAKE_SOURCE_DIR}/Cadence_v287.exe)
    add_executable(Cadence_v286 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v286 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v286 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v286 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v286>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v286>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v286>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v286>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v286>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v286> ${CMAKE_SOURCE_DIR}/Cadence_v286.exe)
    add_executable(Cadence_v285 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v285 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v285 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v285 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v285>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v285>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v285>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v285>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v285>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v285> ${CMAKE_SOURCE_DIR}/Cadence_v285.exe)
    add_executable(Cadence_v284 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v284 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v284 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v284 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v284>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v284>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v284>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v284>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v284>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v284> ${CMAKE_SOURCE_DIR}/Cadence_v284.exe)
    add_executable(Cadence_v283 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v283 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v283 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v283 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v283>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v283>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v283>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v283>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v283>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v283> ${CMAKE_SOURCE_DIR}/Cadence_v283.exe)
    add_executable(Cadence_v282 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v282 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v282 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v282 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v282>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v282>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v282>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v282>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v282>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v282> ${CMAKE_SOURCE_DIR}/Cadence_v282.exe)
    add_executable(Cadence_v281 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v281 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v281 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v281 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v281>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v281>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v281>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v281>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v281>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v281> ${CMAKE_SOURCE_DIR}/Cadence_v281.exe)
    add_executable(Cadence_v280 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v280 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v280 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v280 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v280>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v280>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v280>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v280>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v280>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v280> ${CMAKE_SOURCE_DIR}/Cadence_v280.exe)
    add_executable(Cadence_v279 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v279 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v279 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v279 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v279>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v279>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v279>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v279>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v279>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v279> ${CMAKE_SOURCE_DIR}/Cadence_v279.exe)
    add_executable(Cadence_v278 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v278 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v278 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v278 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v278>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v278>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v278>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v278>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v278>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v278> ${CMAKE_SOURCE_DIR}/Cadence_v278.exe)
    add_executable(Cadence_v277 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v277 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v277 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v277 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v277>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v277>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v277> ${CMAKE_SOURCE_DIR}/Cadence_v277.exe)
    add_executable(Cadence_v276 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v276 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v276 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v276 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v276>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v276>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v276>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v276>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v276>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v276> ${CMAKE_SOURCE_DIR}/Cadence_v276.exe)
    add_executable(Cadence_v275 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v275 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v275 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v275 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v275>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v275>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v275>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v275>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v275>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v275> ${CMAKE_SOURCE_DIR}/Cadence_v275.exe)
    add_executable(Cadence_v274 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v274 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v274 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v274 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v274>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v274>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v274>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v274>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v274>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v274> ${CMAKE_SOURCE_DIR}/Cadence_v274.exe)
    add_executable(Cadence_v273 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v273 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v273 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v273 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v273>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v273>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v273>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v273>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v273>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v273> ${CMAKE_SOURCE_DIR}/Cadence_v273.exe)
    add_executable(Cadence_v272 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v272 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v272 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v272 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v272>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v272>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v272>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v272>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v272>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v272> ${CMAKE_SOURCE_DIR}/Cadence_v272.exe)
    add_executable(Cadence_v271 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v271 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v271 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v271 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v271>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v271>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v271>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v271>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v271>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v271> ${CMAKE_SOURCE_DIR}/Cadence_v271.exe)
    add_executable(Cadence_v270 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v270 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v270 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v270 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v270>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v270>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v270>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v270>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v270>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v270> ${CMAKE_SOURCE_DIR}/Cadence_v270.exe)
    add_executable(Cadence_v269 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v269 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v269 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v269 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v269>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v269>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v269>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v269>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v269>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v269> ${CMAKE_SOURCE_DIR}/Cadence_v269.exe)
    add_executable(Cadence_v268 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v268 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v268 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v268 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v268>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v268>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v268>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v268>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v268>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v268> ${CMAKE_SOURCE_DIR}/Cadence_v268.exe)
    add_executable(Cadence_v267 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v267 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v267 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v267 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v267>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v267>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v267>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v267>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v267>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v267> ${CMAKE_SOURCE_DIR}/Cadence_v267.exe)
    add_executable(yy_v5_tests tests/YyV5Tests.cpp)
    add_executable(workflow_v267_tests tests/WorkflowV267Tests.cpp)
    target_compile_options(workflow_v267_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(workflow_v267_tests PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_test(NAME workflow_v267_tests COMMAND workflow_v267_tests)
    target_include_directories(yy_v5_tests PRIVATE src)
    add_test(NAME yy_v5_tests COMMAND yy_v5_tests)
    add_executable(Cadence_v265 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v265 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v265 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v265 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v265>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v265>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v265>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v265>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v265>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v265> ${CMAKE_SOURCE_DIR}/Cadence_v265.exe)
    add_executable(Cadence_v264 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v264 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v264 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v264 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v264>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v264>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v264>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v264>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v264>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v264> ${CMAKE_SOURCE_DIR}/Cadence_v264.exe)
    add_executable(Cadence_v263 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v263 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v263 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v263 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v263>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v263>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v263>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v263>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v263>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v263> ${CMAKE_SOURCE_DIR}/Cadence_v263.exe)
    add_executable(Cadence_v262 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v262 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v262 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v262 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v262>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v262>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v262>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v262>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v262>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v262> ${CMAKE_SOURCE_DIR}/Cadence_v262.exe)
    add_executable(Cadence_v261 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v261 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v261 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v261 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v261>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v261>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v261>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v261>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v261>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v261> ${CMAKE_SOURCE_DIR}/Cadence_v261.exe)
    add_executable(Cadence_v260 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v260 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v260 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v260 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v260>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v260>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v260>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v260>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v260>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v260> ${CMAKE_SOURCE_DIR}/Cadence_v260.exe)
    add_executable(Cadence_v259 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v259 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v259 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v259 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v259>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v259>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v259>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v259>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v259>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v259> ${CMAKE_SOURCE_DIR}/Cadence_v259.exe)
    add_executable(Cadence_v258 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v258 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v258 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v258 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v258>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v258>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v258>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v258>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v258>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v258> ${CMAKE_SOURCE_DIR}/Cadence_v258.exe)
    add_executable(Cadence_v257 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v257 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v257 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v257 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v257>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v257>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v257>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v257>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v257>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v257> ${CMAKE_SOURCE_DIR}/Cadence_v257.exe)
    add_executable(Cadence_v256 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v256 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v256 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v256 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v256>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v256>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v256>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v256>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v256>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v256> ${CMAKE_SOURCE_DIR}/Cadence_v256.exe)
    add_executable(Cadence_v255 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v255 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v255 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v255 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v255>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v255>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v255>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v255>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v255>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v255> ${CMAKE_SOURCE_DIR}/Cadence_v255.exe)
    add_executable(map_package_isolation_audit tools/MapPackageIsolationAudit.cpp)
    target_link_libraries(map_package_isolation_audit PRIVATE cast_glb_map cast_scene cast_core cast_take)
    add_test(NAME map_package_isolation_tests COMMAND map_package_isolation_audit)
    add_executable(replay_preparation_audit EXCLUDE_FROM_ALL tools/ReplayPreparationAudit.cpp)
    add_executable(replay_camera_audit EXCLUDE_FROM_ALL tools/ReplayCameraAudit.cpp)
    target_compile_options(replay_camera_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(replay_camera_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(standalone_replay_return_audit EXCLUDE_FROM_ALL tools/StandaloneReplayReturnAudit.cpp)
    target_compile_options(standalone_replay_return_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(standalone_replay_return_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(replay_preparation_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(replay_preparation_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v254 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v254 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v254 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v254 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v254>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v254>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v254>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v254>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v254>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v254> ${CMAKE_SOURCE_DIR}/Cadence_v254.exe)
    add_executable(Cadence_v253 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v253 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v253 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v253 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v253>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v253>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v253>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v253>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v253>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v253> ${CMAKE_SOURCE_DIR}/Cadence_v253.exe)
    add_executable(Cadence_v252 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v252 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v252 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v252 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v252>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v252>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v252>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v252>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v252>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v252> ${CMAKE_SOURCE_DIR}/Cadence_v252.exe)
    add_executable(Cadence_v251 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v251 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v251 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v251 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v251>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v251>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v251>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v251>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v251>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v251> ${CMAKE_SOURCE_DIR}/Cadence_v251.exe)
    add_executable(Cadence_v250 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v250 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v250 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v250 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v250>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v250>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v250>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v250>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v250>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v250> ${CMAKE_SOURCE_DIR}/Cadence_v250.exe)
    add_executable(Cadence_v249 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v249 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v249 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v249 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v249>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v249>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v249>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v249>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v249>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v249> ${CMAKE_SOURCE_DIR}/Cadence_v249.exe)
    add_executable(Cadence_v248 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v248 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v248 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v248 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v248>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v248>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v248>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v248>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v248>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v248> ${CMAKE_SOURCE_DIR}/Cadence_v248.exe)
    add_executable(Cadence_v247 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v247 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v247 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v247 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v247>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v247>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v247>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v247>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v247>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v247> ${CMAKE_SOURCE_DIR}/Cadence_v247.exe)
    add_executable(Cadence_v246 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v246 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v246 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v246 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v246>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v246>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v246>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v246>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v246>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v246> ${CMAKE_SOURCE_DIR}/Cadence_v246.exe)
    add_executable(Cadence_v245 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v245 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v245 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v245 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v245>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v245>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v245>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v245>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v245>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v245> ${CMAKE_SOURCE_DIR}/Cadence_v245.exe)
    add_executable(Cadence_v244 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v244 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v244 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v244 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v244>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v244>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v244>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v244>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v244>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v244> ${CMAKE_SOURCE_DIR}/Cadence_v244.exe)
    target_compile_options(Cadence_v243 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v243 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v243 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v243>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v243>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v243>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v243>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v243>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v243> ${CMAKE_SOURCE_DIR}/Cadence_v243.exe)
    target_compile_options(Cadence_v242 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v242 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v242 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v242>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v242>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v242>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v242>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v242>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v242> ${CMAKE_SOURCE_DIR}/Cadence_v242.exe)
    target_compile_options(Cadence_v241 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v241 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v241 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v241>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v241>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v241>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v241>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v241>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/external/miniz/LICENSE" "$<TARGET_FILE_DIR:Cadence_v241>/tools/licenses/miniz.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v241> ${CMAKE_SOURCE_DIR}/Cadence_v241.exe)
    target_compile_options(Cadence_v240 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v240 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v240 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v240>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v240>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v240>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v240>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v240>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v240> ${CMAKE_SOURCE_DIR}/Cadence_v240.exe)
    add_executable(viewmodel_projection_audit EXCLUDE_FROM_ALL tools/ViewmodelProjectionAudit.cpp)
    add_executable(demo_set_replay_audit EXCLUDE_FROM_ALL tools/DemoSetReplayAudit.cpp)
    target_compile_options(demo_set_replay_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(demo_set_replay_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(viewmodel_projection_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(imported_runtime_audit EXCLUDE_FROM_ALL tools/ImportedRuntimeAudit.cpp)
    add_executable(animation_set_audit EXCLUDE_FROM_ALL tools/AnimationSetAudit.cpp)
    target_compile_options(animation_set_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(animation_set_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(imported_cross_replay_audit EXCLUDE_FROM_ALL tools/ImportedCrossReplayAudit.cpp)
    add_executable(imported_world_replay_audit EXCLUDE_FROM_ALL tools/ImportedWorldReplayAudit.cpp)
    target_compile_options(imported_world_replay_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(imported_world_replay_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(imported_cross_replay_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(imported_cross_replay_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(imported_visual_audit EXCLUDE_FROM_ALL tools/ImportedVisualAudit.cpp)
    add_executable(imported_view_fit_audit EXCLUDE_FROM_ALL tools/ImportedViewFitAudit.cpp)
    add_executable(viewhand_matrix_audit EXCLUDE_FROM_ALL tools/ViewHandMatrixAudit.cpp)
    target_link_libraries(viewhand_matrix_audit PRIVATE cast_core cast_scene cast_assets)
    add_executable(viewhand_runtime_audit EXCLUDE_FROM_ALL tools/ViewHandRuntimeAudit.cpp)
    target_compile_options(viewhand_runtime_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(viewhand_runtime_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(cross_game_benchmark EXCLUDE_FROM_ALL tools/CrossGameBenchmark.cpp)
    add_executable(cross_game_visual_v277 EXCLUDE_FROM_ALL tools/CrossGameBenchmark.cpp)
    target_compile_options(cross_game_visual_v277 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_compile_definitions(cross_game_visual_v277 PRIVATE CADENCE_CROSS_CURRENT_REVIEW=1)
    target_link_libraries(cross_game_visual_v277 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(cross_game_benchmark PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(cross_game_benchmark PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(cross_game_failure_probe EXCLUDE_FROM_ALL tools/CrossGameFailureProbe.cpp)
    target_compile_options(cross_game_failure_probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(cross_game_failure_probe PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(imported_body_visual_audit EXCLUDE_FROM_ALL tools/ImportedBodyVisualAudit.cpp)
    target_link_libraries(imported_body_visual_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(imported_world_grip_audit EXCLUDE_FROM_ALL tools/ImportedWorldGripAudit.cpp)
    add_executable(theme_settings_tests tests/ThemeSettingsTests.cpp)
    target_link_libraries(theme_settings_tests PRIVATE cast_take imgui_backend OpenGL::GL)
    add_test(NAME theme_settings_tests COMMAND theme_settings_tests)
    target_link_libraries(imported_world_grip_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    target_link_libraries(imported_view_fit_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(imported_uv_audit EXCLUDE_FROM_ALL tools/ImportedUvAudit.cpp)
    target_link_libraries(imported_uv_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    target_link_libraries(imported_visual_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    target_compile_options(imported_runtime_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(imported_runtime_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v229 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v229 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET Cadence_v229 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v229>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/skyboxes" "$<TARGET_FILE_DIR:Cadence_v229>/skyboxes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v229>/tools"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/tools/ffmpeg.exe" "$<TARGET_FILE_DIR:Cadence_v229>/tools/ffmpeg.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/tools/licenses" "$<TARGET_FILE_DIR:Cadence_v229>/tools/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v229> ${CMAKE_SOURCE_DIR}/Cadence_v229.exe)
    target_link_libraries(movement_preset_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v203_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v203_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_executable(yy_playback_audit EXCLUDE_FROM_ALL tools/YyPlaybackAudit.cpp)
    add_executable(iw4_yy_playback_audit EXCLUDE_FROM_ALL tools/Iw4YyPlaybackAudit.cpp)
    add_executable(legacy_source_movement_audit EXCLUDE_FROM_ALL tools/LegacySourceMovementAudit.cpp)
    target_compile_options(legacy_source_movement_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(legacy_source_movement_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(imported_world_convention_audit EXCLUDE_FROM_ALL tools/ImportedWorldConventionAudit.cpp)
    target_link_libraries(imported_world_convention_audit PRIVATE cast_core cast_scene cast_assets cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    target_compile_options(iw4_yy_playback_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(iw4_yy_playback_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(t6_magazine_audit EXCLUDE_FROM_ALL tools/T6MagazineAudit.cpp)
    target_compile_options(t6_magazine_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(t6_magazine_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(yy_playback_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(yy_playback_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(bot_playback_audit EXCLUDE_FROM_ALL tools/BotPlaybackAudit.cpp)
    add_executable(arsenal_navigation_audit EXCLUDE_FROM_ALL tools/ArsenalNavigationAudit.cpp)
    target_compile_options(arsenal_navigation_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(arsenal_navigation_audit PRIVATE cast_scene cast_weapon cast_glb_map)
    target_compile_options(bot_playback_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(bot_playback_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(putaway_world_audit EXCLUDE_FROM_ALL tools/PutawayWorldAudit.cpp)
    target_compile_options(putaway_world_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(putaway_world_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(world_locomotion_audit EXCLUDE_FROM_ALL tools/WorldLocomotionAudit.cpp)
    target_compile_options(world_locomotion_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(world_locomotion_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(world_motion_bind_audit EXCLUDE_FROM_ALL tools/WorldMotionBindAudit.cpp)
    target_compile_options(world_motion_bind_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(world_motion_bind_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(Cadence_v173 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v173 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v173 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v173 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v173>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v173> ${CMAKE_SOURCE_DIR}/Cadence_v173.exe)
    add_executable(Cadence_v172 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v172 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v172 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v172 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v172>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v172> ${CMAKE_SOURCE_DIR}/Cadence_v172.exe)
    target_compile_options(Cadence_v171 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v171 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v171 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v171>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v171> ${CMAKE_SOURCE_DIR}/Cadence_v171.exe)
    add_executable(loading_audit EXCLUDE_FROM_ALL tools/LoadingAudit.cpp)
    add_executable(batch176_audit EXCLUDE_FROM_ALL tools/Batch176Audit.cpp)
    add_executable(codm_fire_audit EXCLUDE_FROM_ALL tools/CodmFireAudit.cpp)
    add_executable(character_assembler_audit EXCLUDE_FROM_ALL tools/CharacterAssemblerAudit.cpp)
    add_executable(pointblank_cod_hands_audit EXCLUDE_FROM_ALL tools/PointBlankCodHandsAudit.cpp)
    target_compile_options(pointblank_cod_hands_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(pointblank_cod_hands_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(character_assembler_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(character_assembler_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(codm_fire_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(codm_fire_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(sniper_clip_audit EXCLUDE_FROM_ALL tools/SniperClipAudit.cpp)
    target_compile_options(sniper_clip_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(sniper_clip_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(batch176_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(batch176_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(replay_flow_audit EXCLUDE_FROM_ALL tools/ReplayFlowAudit.cpp)
    add_executable(animation_editor_audit EXCLUDE_FROM_ALL tools/AnimationEditorAudit.cpp)
    target_compile_options(animation_editor_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(animation_editor_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(replay_flow_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(replay_flow_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(loading_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(loading_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v170 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v170 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v170 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v170>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v170> ${CMAKE_SOURCE_DIR}/Cadence_v170.exe)
    target_compile_options(Cadence_v169 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v169 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v169 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v169>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v169> ${CMAKE_SOURCE_DIR}/Cadence_v169.exe)
    target_compile_options(Cadence_v168 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v168 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v168 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v168>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v168> ${CMAKE_SOURCE_DIR}/Cadence_v168.exe)
    add_executable(Cadence_v166 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v166 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v166 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v166 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v166>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v166> ${CMAKE_SOURCE_DIR}/Cadence_v166.exe)
    add_executable(Cadence_v165 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_compile_options(Cadence_v165 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v165 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v165 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v165>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v165> ${CMAKE_SOURCE_DIR}/Cadence_v165.exe)
    if(CAST_BUILD_TESTS)
        add_executable(bot_team_selection_tests tests/BotTeamSelectionTests.cpp)
        target_compile_options(bot_team_selection_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
        target_link_libraries(bot_team_selection_tests PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
        add_test(NAME bot_team_selection_tests COMMAND bot_team_selection_tests)
    endif()
    target_link_libraries(water_visual_check PRIVATE stage_renderer cast_take cast_glb_map)
    target_link_libraries(rain_visual_check PRIVATE stage_renderer cast_take cast_glb_map)
    target_compile_options(Cadence_v160 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v160 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v160 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v160>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v160> ${CMAKE_SOURCE_DIR}/Cadence_v160.exe)
    target_compile_options(Cadence_v159 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v159 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v159 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v159>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v159> ${CMAKE_SOURCE_DIR}/Cadence_v159.exe)
    target_compile_options(Cadence_v158 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v158 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v158 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v158>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v158> ${CMAKE_SOURCE_DIR}/Cadence_v158.exe)
    add_executable(grip_landmark_probe EXCLUDE_FROM_ALL tools/GripLandmarkProbe.cpp)
    add_executable(world_assembly_probe EXCLUDE_FROM_ALL tools/WorldAssemblyProbe.cpp)
    target_link_libraries(world_assembly_probe PRIVATE cast_scene)
    target_link_libraries(grip_landmark_probe PRIVATE cast_scene)
    target_compile_options(Cadence_v157 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v157 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v157 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v157>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v157> ${CMAKE_SOURCE_DIR}/Cadence_v157.exe)
    add_executable(cadence_weapon_calibrator EXCLUDE_FROM_ALL tools/GripCalibration.cpp)
    set_target_properties(cadence_weapon_calibrator PROPERTIES OUTPUT_NAME "cadence weapon calibrator")
    target_compile_options(cadence_weapon_calibrator PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj> $<$<CXX_COMPILER_ID:MSVC>:/Od>)
    target_link_libraries(cadence_weapon_calibrator PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_custom_command(TARGET cadence_weapon_calibrator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:cadence_weapon_calibrator> "${CMAKE_SOURCE_DIR}/cadence weapon calibrator.exe")
    target_compile_options(Cadence_v156 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v156 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v156 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v156>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v156> ${CMAKE_SOURCE_DIR}/Cadence_v156.exe)
    add_executable(v156_rig_probe EXCLUDE_FROM_ALL tools/V156RigProbe.cpp)
    target_link_libraries(v156_rig_probe PRIVATE cast_scene)
    target_compile_options(Cadence_v155 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v155 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v155 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v155>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v155> ${CMAKE_SOURCE_DIR}/Cadence_v155.exe)
    add_executable(v155_recording_check EXCLUDE_FROM_ALL tools/V155RecordingCheck.cpp)
    add_executable(world_grip_audit EXCLUDE_FROM_ALL tools/WorldGripAudit.cpp)
    add_executable(replay_edit_audit EXCLUDE_FROM_ALL tools/ReplayEditAudit.cpp)
    target_link_libraries(replay_edit_audit PRIVATE cast_take cast_scene)
    target_compile_options(world_grip_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj> $<$<CXX_COMPILER_ID:MSVC>:/Od>)
    target_link_libraries(world_grip_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v155_recording_check PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v155_recording_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v154 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v154 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v154 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v154>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v154> ${CMAKE_SOURCE_DIR}/Cadence_v154.exe)
    target_compile_options(Cadence_v153 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v153 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v153 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v153>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v153> ${CMAKE_SOURCE_DIR}/Cadence_v153.exe)
    add_executable(v154_map_check EXCLUDE_FROM_ALL tools/V154MapCheck.cpp)
    target_link_libraries(v154_map_check PRIVATE stage_renderer cast_glb_map cast_take imgui_backend OpenGL::GL)
    add_executable(v153_checks EXCLUDE_FROM_ALL tools/V153Checks.cpp)
    target_compile_options(v153_checks PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj> $<$<CXX_COMPILER_ID:MSVC>:/Od>)
    target_link_libraries(v153_checks PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v152 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v152 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v152 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v152>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v152> ${CMAKE_SOURCE_DIR}/Cadence_v152.exe)
    add_executable(surface_penetration_tests tests/SurfacePenetrationTests.cpp)
    target_link_libraries(surface_penetration_tests PRIVATE cast_glb_map)
    add_test(NAME surface_penetration_tests COMMAND surface_penetration_tests)
    target_compile_options(Cadence_v151 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v151 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v151 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v151>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v151> ${CMAKE_SOURCE_DIR}/Cadence_v151.exe)
    add_executable(v151_grip EXCLUDE_FROM_ALL tools/V151Grip.cpp)
    target_compile_options(v151_grip PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj> $<$<CXX_COMPILER_ID:MSVC>:/Od>)
    target_link_libraries(v151_grip PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v150 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v150 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v150 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets" "$<TARGET_FILE_DIR:Cadence_v150>/Cadence Assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v150> ${CMAKE_SOURCE_DIR}/Cadence_v150.exe)
    target_compile_options(Cadence_v149 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v149 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v149 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets/scopes" "$<TARGET_FILE_DIR:Cadence_v149>/Cadence Assets/scopes"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Cadence Assets/sky" "$<TARGET_FILE_DIR:Cadence_v149>/Cadence Assets/sky"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v149> ${CMAKE_SOURCE_DIR}/Cadence_v149.exe)
    add_executable(v150_characters EXCLUDE_FROM_ALL tools/V150Characters.cpp)
    target_link_libraries(v150_characters PRIVATE cast_scene cast_assets)
    add_executable(v149_probe EXCLUDE_FROM_ALL tools/V149Probe.cpp)
    target_link_libraries(v149_probe PRIVATE cast_scene)
    add_executable(v149_world EXCLUDE_FROM_ALL tools/V149World.cpp)
    target_compile_options(v149_world PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v149_world PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(Cadence_v148 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v148 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v148 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v148>/Cadence Assets/scopes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/Cadence Assets/scopes/scope_overlay_dsr50_1024.png" "$<TARGET_FILE_DIR:Cadence_v148>/Cadence Assets/scopes/scope_overlay_dsr50_1024.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v148> ${CMAKE_SOURCE_DIR}/Cadence_v148.exe)
    target_compile_options(Cadence_v147 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v147 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v147 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v147>/Cadence Assets/scopes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/Cadence Assets/scopes/scope_overlay_dsr50_1024.png" "$<TARGET_FILE_DIR:Cadence_v147>/Cadence Assets/scopes/scope_overlay_dsr50_1024.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v147> ${CMAKE_SOURCE_DIR}/Cadence_v147.exe)
    target_compile_options(Cadence_v146 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v146 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v146 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Cadence_v146>/Cadence Assets/scopes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/Cadence Assets/scopes/scope_overlay_dsr50_1024.png" "$<TARGET_FILE_DIR:Cadence_v146>/Cadence Assets/scopes/scope_overlay_dsr50_1024.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v146> ${CMAKE_SOURCE_DIR}/Cadence_v146.exe)
    target_compile_options(Cadence_v145 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v145 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v145 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v145> ${CMAKE_SOURCE_DIR}/Cadence_v145.exe)
    target_compile_options(Cadence_v144 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v144 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v144 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v144> ${CMAKE_SOURCE_DIR}/Cadence_v144.exe)
    target_compile_options(Cadence_v143 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v143 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v143 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v143> ${CMAKE_SOURCE_DIR}/Cadence_v143.exe)
    target_compile_options(Cadence_v142 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(Cadence_v142 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v142 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v142> ${CMAKE_SOURCE_DIR}/Cadence_v142.exe)
    add_executable(pointblank_visual EXCLUDE_FROM_ALL tools/PointBlankVisual.cpp)
    add_executable(pointblank_source_audit EXCLUDE_FROM_ALL tools/PointBlankSourceAudit.cpp)
    target_link_libraries(pointblank_source_audit PRIVATE cast_scene cast_core)
    target_link_libraries(pointblank_visual PRIVATE stage_renderer cast_scene cast_take imgui_backend OpenGL::GL)
    target_link_libraries(Cadence_v134 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v134 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v134> ${CMAKE_SOURCE_DIR}/Cadence_v134.exe)
    add_executable(Cadence_v133 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v133 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v133 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v133> ${CMAKE_SOURCE_DIR}/Cadence_v133.exe)
    add_executable(Cadence_v126 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v126 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v126 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v126> ${CMAKE_SOURCE_DIR}/Cadence_v126.exe)
    add_executable(Cadence_v125 EXCLUDE_FROM_ALL src/app/main.cpp)
    target_link_libraries(Cadence_v125 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v125 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v125> ${CMAKE_SOURCE_DIR}/Cadence_v125.exe)
    target_link_libraries(Cadence_v124 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v124 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v124> ${CMAKE_SOURCE_DIR}/Cadence_v124.exe)
    target_link_libraries(Cadence_v123 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v123 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v123> ${CMAKE_SOURCE_DIR}/Cadence_v123.exe)
    target_link_libraries(Cadence_v122 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v122 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v122> ${CMAKE_SOURCE_DIR}/Cadence_v122.exe)
    target_link_libraries(Cadence_v76 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v76 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v76> ${CMAKE_SOURCE_DIR}/Cadence_v76.exe)
    target_link_libraries(Cadence_v75 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v75 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v75> ${CMAKE_SOURCE_DIR}/Cadence_v75.exe)
    target_link_libraries(Cadence_v74 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v74 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v74> ${CMAKE_SOURCE_DIR}/Cadence_v74.exe)
    target_link_libraries(Cadence_v73 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v73 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v73> ${CMAKE_SOURCE_DIR}/Cadence_v73.exe)
    add_executable(shot_trail_visual_check EXCLUDE_FROM_ALL tools/ShotTrailVisualCheck.cpp)
    add_executable(heaven_smile_visual_check EXCLUDE_FROM_ALL tools/HeavenSmileVisualCheck.cpp)
    target_link_libraries(heaven_smile_visual_check PRIVATE cast_core cast_scene cast_take stage_renderer glfw OpenGL::GL)
    target_link_libraries(shot_trail_visual_check PRIVATE cast_core cast_scene cast_take stage_renderer glfw OpenGL::GL)
    target_link_libraries(Cadence_v68 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v68 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v68> ${CMAKE_SOURCE_DIR}/Cadence_v68.exe)
    target_link_libraries(Cadence_v63 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v63 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v63> ${CMAKE_SOURCE_DIR}/Cadence_v63.exe)
    target_link_libraries(Cadence_v62 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v62 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Cadence_v62> ${CMAKE_SOURCE_DIR}/Cadence_v62.exe)
    target_link_libraries(Cadence_v61 PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL)
    add_custom_command(TARGET Cadence_v61 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Cadence_v61>
        ${CMAKE_SOURCE_DIR}/Cadence_v61.exe)
    if(CAST_BUILD_TESTS)
        add_executable(glb_map_tests tests/GlbMapTests.cpp)
        target_link_libraries(glb_map_tests PRIVATE cast_glb_map)
        add_test(NAME glb_map_tests COMMAND glb_map_tests ${CMAKE_CURRENT_SOURCE_DIR}/external/cgltf/fuzz/data/Box.glb)
        add_executable(c2m_map_tests tests/C2MMapTests.cpp)
        target_link_libraries(c2m_map_tests PRIVATE cast_glb_map)
        add_test(NAME c2m_map_tests COMMAND c2m_map_tests)
    endif()
    if(WIN32)
        target_link_libraries(cast_viewer PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v12 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v13 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v14 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v15 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v16 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v17 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v18 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v19 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v20 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v21 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v22 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v23 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v24 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v25 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v26 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v27 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v28 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v29 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v30 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v31 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v32 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v33 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v34 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v35 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v36 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v37 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v38 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v39 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v40 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v41 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v42 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v43 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v44 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v45 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v46 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v47 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v48 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v49 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v50 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v51 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v52 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v53 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v54 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v55 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v56 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v57 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v58 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v59 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v60 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v61 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v62 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v63 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v64 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v65 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v66 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v67 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v68 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v69 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v70 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v71 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v72 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v73 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v74 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v75 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v76 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v77 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v78 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v79 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v80 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v81 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v82 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v83 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v84 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v85 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v86 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v87 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v88 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v89 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v90 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v91 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v92 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v93 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v94 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v95 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v96 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v97 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v98 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v99 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v100 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v101 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v102 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v103 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v104 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v105 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v106 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v107 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v108 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v109 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v110 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v111 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v112 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v113 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v114 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v115 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v116 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v117 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v118 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v119 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v120 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v121 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v122 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v123 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v124 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v125 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v126 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v127 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v128 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v129 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v130 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v131 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v132 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v133 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v134 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v135 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v136 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v137 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v138 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v139 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v140 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v141 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v142 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v143 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v144 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v145 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v146 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v147 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v148 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v149 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v150 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v151 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v152 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v153 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v154 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v155 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v156 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v157 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v158 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v159 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v160 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v161 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v162 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v163 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v164 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v165 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v166 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v167 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v168 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v169 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v170 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v171 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v172 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v173 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v174 PRIVATE comdlg32 shell32 ole32)
        target_link_libraries(Cadence_v175 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v176 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v177 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v178 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v179 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v180 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v181 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v182 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v183 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v184 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v185 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v186 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v187 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v188 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v189 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v190 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v191 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v192 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v193 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v194 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v195 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v196 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v197 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v198 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v199 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v200 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v201 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v202 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v203 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v204 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v205 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v206 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v207 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v208 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v209 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v210 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v211 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v212 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v213 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v214 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v215 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v216 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v217 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v218 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v219 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v220 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v221 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v222 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v223 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(Cadence_v224 PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(v203_audit PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(v202_audit PRIVATE comdlg32 shell32 ole32)
    add_executable(codm_foreign_hands_audit EXCLUDE_FROM_ALL tools/CodmForeignHandsAudit.cpp)
    target_compile_options(codm_foreign_hands_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(codm_foreign_hands_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(rain_app_check PRIVATE comdlg32 shell32 ole32)
    target_link_libraries(rain_preset_audit PRIVATE comdlg32 shell32 ole32)
        if(TARGET bot_team_selection_tests)
            target_link_libraries(bot_team_selection_tests PRIVATE comdlg32 shell32 ole32)
        endif()
    endif()
endif()

if(CAST_BUILD_TESTS)
    enable_testing()
    add_executable(visual_mismatch_tests tests/VisualMismatchTests.cpp)
    target_link_libraries(visual_mismatch_tests PRIVATE cast_scene cast_take cast_weapon)
    add_test(NAME visual_mismatch_tests COMMAND visual_mismatch_tests)
    add_executable(stability_v162_tests tests/StabilityV162Tests.cpp)
    target_link_libraries(stability_v162_tests PRIVATE cast_scene cast_take)
    add_test(NAME stability_v162_tests COMMAND stability_v162_tests)
    add_executable(bot_lifecycle_tests tests/BotLifecycleTests.cpp)
    add_executable(mantle_pursuit_tests tests/MantlePursuitTests.cpp)
    add_executable(player_map_movement_tests tests/PlayerMapMovementTests.cpp)
    target_link_libraries(player_map_movement_tests PRIVATE cast_glb_map)
    add_test(NAME player_map_movement_tests COMMAND player_map_movement_tests)
    target_link_libraries(mantle_pursuit_tests PRIVATE cast_glb_map)
    add_test(NAME mantle_pursuit_tests COMMAND mantle_pursuit_tests)
    target_link_libraries(bot_lifecycle_tests PRIVATE cast_scene cast_assets)
    add_test(NAME bot_lifecycle_tests COMMAND bot_lifecycle_tests)
    add_executable(collision_range_tests tests/CollisionRangeTests.cpp)
    add_executable(authored_gameplay_tests tests/AuthoredGameplayTests.cpp)
    target_link_libraries(authored_gameplay_tests PRIVATE cast_glb_map)
    add_test(NAME authored_gameplay_tests COMMAND authored_gameplay_tests)
    target_link_libraries(collision_range_tests PRIVATE cast_glb_map)
    add_test(NAME collision_range_tests COMMAND collision_range_tests)
    add_executable(prepared_animation_library_tests tests/PreparedAnimationLibraryTests.cpp)
    target_link_libraries(prepared_animation_library_tests PRIVATE cast_scene)
    add_test(NAME prepared_animation_library_tests COMMAND prepared_animation_library_tests)
    add_executable(bo2_loadout_defaults_tests tests/Bo2LoadoutDefaultsTests.cpp)
    add_executable(weapon_picker_cache_tests tests/WeaponPickerCacheTests.cpp)
    target_link_libraries(weapon_picker_cache_tests PRIVATE cast_assets)
    if(MSVC)
        target_compile_options(weapon_picker_cache_tests PRIVATE /UNDEBUG)
    endif()
    add_test(NAME weapon_picker_cache_tests COMMAND weapon_picker_cache_tests)
    target_link_libraries(bo2_loadout_defaults_tests PRIVATE cast_assets)
    add_test(NAME bo2_loadout_defaults_tests COMMAND bo2_loadout_defaults_tests)
    add_executable(texture_preparation_tests tests/TexturePreparationTests.cpp)
    target_link_libraries(texture_preparation_tests PRIVATE stage_renderer)
    add_test(NAME texture_preparation_tests COMMAND texture_preparation_tests)
    add_executable(static_material_order_tests tests/StaticMaterialOrderTests.cpp)
    target_include_directories(static_material_order_tests PRIVATE src)
    add_test(NAME static_material_order_tests COMMAND static_material_order_tests)
    add_executable(timed_snapshot_cache_tests tests/TimedSnapshotCacheTests.cpp)
    target_include_directories(timed_snapshot_cache_tests PRIVATE src)
    add_test(NAME timed_snapshot_cache_tests COMMAND timed_snapshot_cache_tests)
    add_executable(animation_file_index_tests tests/AnimationFileIndexTests.cpp)
    target_link_libraries(animation_file_index_tests PRIVATE cast_assets)
    add_test(NAME animation_file_index_tests COMMAND animation_file_index_tests)
    add_executable(bot_presentation_tests tests/BotPresentationTests.cpp)
    add_executable(bot_map_physics_tests tests/BotMapPhysicsTests.cpp)
    target_link_libraries(bot_map_physics_tests PRIVATE cast_scene cast_glb_map)
    add_test(NAME bot_map_physics_tests COMMAND bot_map_physics_tests)
    add_executable(bot_map_routing_tests tests/BotMapRoutingTests.cpp)
    target_link_libraries(bot_map_routing_tests PRIVATE cast_scene)
    add_test(NAME bot_map_routing_tests COMMAND bot_map_routing_tests)
    add_executable(bot_sp_behavior_tests tests/BotSpBehaviorTests.cpp)
    target_link_libraries(bot_sp_behavior_tests PRIVATE cast_scene)
    add_test(NAME bot_sp_behavior_tests COMMAND bot_sp_behavior_tests)
    add_executable(bot_sp_variety_tests tests/BotSpVarietyTests.cpp)
    target_link_libraries(bot_sp_variety_tests PRIVATE cast_scene)
    add_test(NAME bot_sp_variety_tests COMMAND bot_sp_variety_tests)
    add_executable(bot_sp_context_tests tests/BotSpContextTests.cpp)
    target_link_libraries(bot_sp_context_tests PRIVATE cast_scene)
    add_test(NAME bot_sp_context_tests COMMAND bot_sp_context_tests)
    add_executable(bot_sp_traversal_tests tests/BotSpTraversalTests.cpp)
    target_link_libraries(bot_sp_traversal_tests PRIVATE cast_scene)
    add_test(NAME bot_sp_traversal_tests COMMAND bot_sp_traversal_tests)
    add_executable(bot_sp_context_runtime_tests tests/BotSpContextRuntimeTests.cpp)
    target_link_libraries(bot_sp_context_runtime_tests PRIVATE cast_scene)
    add_test(NAME bot_sp_context_runtime_tests COMMAND bot_sp_context_runtime_tests)
    add_executable(c2mx_map_tests tests/C2mxMapTests.cpp)
    target_link_libraries(c2mx_map_tests PRIVATE cast_glb_map)
    if(MSVC)
        target_compile_options(c2mx_map_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(c2mx_map_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME c2mx_map_tests COMMAND c2mx_map_tests)
    target_link_libraries(bot_presentation_tests PRIVATE cast_scene)
    add_test(NAME bot_presentation_tests COMMAND bot_presentation_tests)
    add_executable(airborne_animation_tests tests/AirborneAnimationTests.cpp)
    target_link_libraries(airborne_animation_tests PRIVATE cast_scene)
    add_test(NAME airborne_animation_tests COMMAND airborne_animation_tests)
    add_executable(sprint_motion_tests tests/SprintMotionTests.cpp)
    target_link_libraries(sprint_motion_tests PRIVATE cast_scene)
    add_test(NAME sprint_motion_tests COMMAND sprint_motion_tests)
    add_executable(bot_animation_cache_tests tests/BotAnimationCacheTests.cpp)
    target_link_libraries(bot_animation_cache_tests PRIVATE cast_scene cast_core)
    add_test(NAME bot_animation_cache_tests COMMAND bot_animation_cache_tests)
    add_executable(benchmark_support_tests tests/BenchmarkSupportTests.cpp)
    target_include_directories(benchmark_support_tests PRIVATE src)
    add_test(NAME benchmark_support_tests COMMAND benchmark_support_tests)
    add_executable(cast_parser_tests tests/CastParserTests.cpp)
    add_executable(game_preload_tests tests/GamePreloadTests.cpp)
    target_link_libraries(game_preload_tests PRIVATE cast_scene stage_renderer)
    add_test(NAME game_preload_tests COMMAND game_preload_tests)
    target_link_libraries(cast_parser_tests PRIVATE cast_core)
    add_test(NAME cast_parser_tests COMMAND cast_parser_tests)
    add_executable(cast_scene_tests tests/CastSceneTests.cpp)
    add_executable(codm_native_tests tests/CodmNativeTests.cpp)
    add_executable(codm_reference_tests tests/CodmReferenceTests.cpp)
    target_link_libraries(codm_reference_tests PRIVATE cast_scene)
    add_test(NAME codm_reference_tests COMMAND codm_reference_tests)
    target_link_libraries(codm_native_tests PRIVATE cast_scene)
    add_test(NAME codm_native_tests COMMAND codm_native_tests)
    add_executable(source2_retarget_tests tests/Source2RetargetTests.cpp)
    target_link_libraries(source2_retarget_tests PRIVATE cast_scene cast_core)
    add_test(NAME source2_retarget_tests COMMAND source2_retarget_tests)
    target_link_libraries(cast_scene_tests PRIVATE cast_scene)
    add_test(NAME cast_scene_tests COMMAND cast_scene_tests)
    add_executable(cast_take_tests tests/TakeTests.cpp)
    add_executable(world_motion_v116_tests tests/world_motion_v116_test.cpp)
    target_link_libraries(world_motion_v116_tests PRIVATE cast_scene)
    if(MSVC)
        target_compile_options(world_motion_v116_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(world_motion_v116_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME world_motion_v116_tests COMMAND world_motion_v116_tests)
    add_executable(world_weapon_assembly_tests tests/WorldWeaponAssemblyTests.cpp)
    target_link_libraries(world_weapon_assembly_tests PRIVATE cast_scene)
    add_test(NAME world_weapon_assembly_tests COMMAND world_weapon_assembly_tests)
    add_executable(t6_shared_textures_tests tests/T6SharedTexturesTests.cpp)
    target_include_directories(t6_shared_textures_tests PRIVATE src)
    add_test(NAME t6_shared_textures_tests COMMAND t6_shared_textures_tests)
    add_executable(cold_war_world_tests tests/ColdWarWorldTests.cpp)
    target_link_libraries(cold_war_world_tests PRIVATE cast_scene)
    add_test(NAME cold_war_world_tests COMMAND cold_war_world_tests)
    target_link_libraries(cast_take_tests PRIVATE cast_take)
    add_test(NAME cast_take_tests COMMAND cast_take_tests)
    add_executable(cast_asset_tests tests/AssetCatalogTests.cpp)
    target_link_libraries(cast_asset_tests PRIVATE cast_assets)
    add_test(NAME cast_asset_tests COMMAND cast_asset_tests)
    add_executable(iw_presentation_tests tests/IwPresentationTests.cpp)
    target_link_libraries(iw_presentation_tests PRIVATE cast_scene)
    add_test(NAME iw_presentation_tests COMMAND iw_presentation_tests)
    add_executable(source_movement_tests tests/SourceMovementTests.cpp)
    add_executable(reference_movement_tests tests/ReferenceMovementTests.cpp)
    add_executable(css_movement_tests tests/CssMovementTests.cpp)
    target_link_libraries(css_movement_tests PRIVATE cast_scene cast_glb_map)
    add_test(NAME css_movement_tests COMMAND css_movement_tests)
    target_link_libraries(reference_movement_tests PRIVATE cast_scene cast_glb_map)
    add_test(NAME reference_movement_tests COMMAND reference_movement_tests)
    target_link_libraries(source_movement_tests PRIVATE cast_scene cast_glb_map)
    add_test(NAME source_movement_tests COMMAND source_movement_tests)
    add_executable(action_cancel_policy_tests tests/ActionCancelPolicyTests.cpp)
    target_include_directories(action_cancel_policy_tests PRIVATE src)
    add_test(NAME action_cancel_policy_tests COMMAND action_cancel_policy_tests)
    add_executable(weapon_profile_tests tests/WeaponProfileTests.cpp)
    add_executable(presentation487_tests tests/Presentation487Tests.cpp)
    add_executable(score_preview_tests tests/ScorePreviewTests.cpp)
    target_include_directories(score_preview_tests PRIVATE src)
    add_test(NAME score_preview_tests COMMAND score_preview_tests)
    target_link_libraries(presentation487_tests PRIVATE cast_scene)
    add_test(NAME presentation487_tests COMMAND presentation487_tests)
    add_executable(death_ragdoll_tests tests/DeathRagdollTests.cpp)
    target_link_libraries(death_ragdoll_tests PRIVATE cast_scene cast_take)
    if(TARGET box3d::box3d)
        target_compile_definitions(death_ragdoll_tests PRIVATE CADENCE_TEST_WEAPON_DROP)
        target_link_libraries(death_ragdoll_tests PRIVATE cast_glb_map)
    endif()
    add_test(NAME death_ragdoll_tests COMMAND death_ragdoll_tests)
    target_link_libraries(weapon_profile_tests PRIVATE cast_weapon)
    add_test(NAME weapon_profile_tests COMMAND weapon_profile_tests)
    add_executable(bot_actor_tests tests/BotActorTests.cpp)
    target_link_libraries(bot_actor_tests PRIVATE cast_scene)
    add_test(NAME bot_actor_tests COMMAND bot_actor_tests)
    add_executable(cast_navigation_sidecar_tests tests/CastNavigationSidecarTests.cpp)
    target_link_libraries(cast_navigation_sidecar_tests PRIVATE cast_scene)
    add_test(NAME cast_navigation_sidecar_tests COMMAND cast_navigation_sidecar_tests)
    add_executable(dds_loader_tests tests/DdsLoaderTests.cpp)
    add_executable(mesh_uniform_cache_tests tests/MeshUniformCacheTests.cpp)
    add_executable(cull_state_tests tests/CullStateTests.cpp)
    target_include_directories(cull_state_tests PRIVATE src)
    add_test(NAME cull_state_tests COMMAND cull_state_tests)
    add_executable(codm_animation_policy_tests tests/CodmAnimationPolicyTests.cpp)
    add_executable(pointblank_tests tests/PointBlankTests.cpp)
    add_executable(bot_area_ping_tests tests/BotAreaPingTests.cpp)
    add_executable(replay_visibility_tests tests/ReplayVisibilityTests.cpp)
    target_link_libraries(replay_visibility_tests PRIVATE cast_scene)
    add_test(NAME replay_visibility_tests COMMAND replay_visibility_tests)
    add_executable(replay_presentation_tests tests/ReplayPresentationTests.cpp)
    add_executable(replay_asset_relocation_tests tests/ReplayAssetRelocationTests.cpp)
    add_executable(reference_viewhands_tests tests/ReferenceViewhandsTests.cpp)
    target_include_directories(reference_viewhands_tests PRIVATE src)
    add_test(NAME reference_viewhands_tests COMMAND reference_viewhands_tests)
    add_executable(export_identity_tests tests/ExportIdentityTests.cpp)
    target_include_directories(export_identity_tests PRIVATE src)
    target_link_libraries(export_identity_tests PRIVATE cast_scene)
    add_test(NAME export_identity_tests COMMAND export_identity_tests)
    target_include_directories(replay_asset_relocation_tests PRIVATE src)
    add_test(NAME replay_asset_relocation_tests COMMAND replay_asset_relocation_tests)
    target_link_libraries(replay_presentation_tests PRIVATE cast_scene cast_take)
    add_test(NAME replay_presentation_tests COMMAND replay_presentation_tests)
    add_executable(v145_world EXCLUDE_FROM_ALL tools/V145World.cpp)
    add_executable(v145_runtime EXCLUDE_FROM_ALL tools/V145Runtime.cpp)
    target_compile_options(v145_runtime PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v145_runtime PRIVATE cast_core cast_scene cast_assets cast_take cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v145_world PRIVATE cast_core cast_scene cast_assets cast_take stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(bot_area_ping_tests PRIVATE cast_scene cast_weapon)
    add_test(NAME bot_area_ping_tests COMMAND bot_area_ping_tests)
    target_link_libraries(pointblank_tests PRIVATE cast_scene cast_weapon)
    add_test(NAME pointblank_tests COMMAND pointblank_tests)
    add_executable(actor_overlay_tests tests/ActorOverlayTests.cpp)
    add_executable(codm_spawns_tests tests/CodmSpawnsTests.cpp)
    add_executable(capture_v130_check EXCLUDE_FROM_ALL tools/CaptureV130Check.cpp)
    target_link_libraries(capture_v130_check PRIVATE cast_scene shell32 ole32)
    add_executable(v130_integration EXCLUDE_FROM_ALL tools/V130Integration.cpp)
    target_link_libraries(v130_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(v131_integration EXCLUDE_FROM_ALL tools/V131Integration.cpp)
    add_executable(knife_hit_tests tests/KnifeHitTests.cpp)
    target_link_libraries(knife_hit_tests PRIVATE cast_weapon)
    add_test(NAME knife_hit_tests COMMAND knife_hit_tests)
    add_executable(v133_knife_check EXCLUDE_FROM_ALL tools/V133KnifeCheck.cpp)
    add_executable(v134_integration EXCLUDE_FROM_ALL tools/V134Integration.cpp)
    add_executable(v135_knife_integration EXCLUDE_FROM_ALL tools/V135KnifeIntegration.cpp)
    add_executable(pointblank_playback_audit EXCLUDE_FROM_ALL tools/PointBlankPlaybackAudit.cpp)
    add_executable(v136_pointblank_integration EXCLUDE_FROM_ALL tools/V136PointBlankIntegration.cpp)
    target_link_libraries(v136_pointblank_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(v137_pointblank_integration EXCLUDE_FROM_ALL tools/V137PointBlankIntegration.cpp)
    add_executable(v138_dual_integration EXCLUDE_FROM_ALL tools/V138DualIntegration.cpp)
    add_executable(v139_transitions EXCLUDE_FROM_ALL tools/V139Transitions.cpp)
    add_executable(v140_transitions EXCLUDE_FROM_ALL tools/V140Transitions.cpp)
    add_executable(v141_transitions EXCLUDE_FROM_ALL tools/V141Transitions.cpp)
    add_executable(v141_world EXCLUDE_FROM_ALL tools/V141World.cpp)
    add_executable(v142_checks EXCLUDE_FROM_ALL tools/V142Checks.cpp)
    add_executable(v143_world EXCLUDE_FROM_ALL tools/V143World.cpp)
    add_executable(v144_ping EXCLUDE_FROM_ALL tools/V144Ping.cpp)
    target_compile_options(v144_ping PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v144_ping PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v143_world PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v143_world PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v142_checks PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v142_checks PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v141_world PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v141_world PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_compile_options(v141_transitions PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v141_transitions PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v140_transitions PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v139_transitions PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v138_dual_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v137_pointblank_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(pointblank_playback_audit PRIVATE cast_core cast_scene cast_assets cast_weapon)
    target_link_libraries(v135_knife_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v134_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v133_knife_check PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(frame_preparation_v132_tests tests/FramePreparationV132Tests.cpp)
    target_link_libraries(frame_preparation_v132_tests PRIVATE cast_scene)
    add_test(NAME frame_preparation_v132_tests COMMAND frame_preparation_v132_tests)
    add_executable(v132_render_check EXCLUDE_FROM_ALL tools/V132RenderCheck.cpp)
    add_executable(dds_upload_check EXCLUDE_FROM_ALL tools/DdsUploadCheck.cpp)
    add_executable(albedo_pass_check EXCLUDE_FROM_ALL tools/AlbedoPassCheck.cpp)
    add_executable(sky_environment_audit EXCLUDE_FROM_ALL tools/SkyEnvironmentAudit.cpp)
    add_executable(player_body_presentation_audit EXCLUDE_FROM_ALL tools/PlayerBodyPresentationAudit.cpp)
    target_compile_options(player_body_presentation_audit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(player_body_presentation_audit PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(sky_environment_audit PRIVATE cast_scene cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(albedo_pass_check PRIVATE cast_scene cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(dds_upload_check PRIVATE cast_scene cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v132_render_check PRIVATE cast_scene cast_take cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    target_link_libraries(v131_integration PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(low_risk_v131_tests tests/LowRiskV131Tests.cpp)
    target_include_directories(low_risk_v131_tests PRIVATE src)
    add_test(NAME low_risk_v131_tests COMMAND low_risk_v131_tests)
    target_include_directories(codm_spawns_tests PRIVATE src)
    add_test(NAME codm_spawns_tests COMMAND codm_spawns_tests)
    target_include_directories(actor_overlay_tests PRIVATE src)
    add_test(NAME actor_overlay_tests COMMAND actor_overlay_tests)
    add_executable(depth_prores_tests tests/DepthProResTests.cpp)
    target_include_directories(depth_prores_tests PRIVATE src)
    add_test(NAME depth_prores_tests COMMAND depth_prores_tests)
    add_executable(viewport_layout_tests tests/ViewportLayoutTests.cpp)
    add_executable(day_night_tests tests/DayNightTests.cpp)
    target_include_directories(day_night_tests PRIVATE src)
    add_test(NAME day_night_tests COMMAND day_night_tests)
    add_executable(v148_night EXCLUDE_FROM_ALL tools/V148Night.cpp)
    target_compile_options(v148_night PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v148_night PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(v147_cycle EXCLUDE_FROM_ALL tools/V147Cycle.cpp)
    target_compile_options(v147_cycle PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
    target_link_libraries(v147_cycle PRIVATE cast_core cast_scene cast_take cast_assets cast_weapon cast_glb_map stage_renderer imgui_backend OpenGL::GL comdlg32 shell32 ole32)
    add_executable(volumetric_lighting_tests tests/VolumetricLightingTests.cpp)
    add_executable(water_tests tests/WaterTests.cpp)
    add_executable(rain_tests tests/RainTests.cpp)
    target_link_libraries(rain_tests PRIVATE cast_glb_map)
    add_test(NAME rain_tests COMMAND rain_tests)
    target_include_directories(water_tests PRIVATE src)
    add_test(NAME water_tests COMMAND water_tests)
    target_include_directories(volumetric_lighting_tests PRIVATE src)
    add_test(NAME volumetric_lighting_tests COMMAND volumetric_lighting_tests)
    add_executable(procedural_view_motion_tests tests/ProceduralViewMotionTests.cpp)
    target_include_directories(procedural_view_motion_tests PRIVATE src)
    add_test(NAME procedural_view_motion_tests COMMAND procedural_view_motion_tests)
    target_include_directories(viewport_layout_tests PRIVATE src)
    add_test(NAME viewport_layout_tests COMMAND viewport_layout_tests)
    target_link_libraries(codm_animation_policy_tests PRIVATE cast_scene cast_weapon)
    add_test(NAME codm_animation_policy_tests COMMAND codm_animation_policy_tests)
    target_include_directories(mesh_uniform_cache_tests PRIVATE src)
    add_test(NAME mesh_uniform_cache_tests COMMAND mesh_uniform_cache_tests)
    target_link_libraries(dds_loader_tests PRIVATE stage_renderer)
    add_test(NAME dds_loader_tests COMMAND dds_loader_tests)
    add_executable(create_a_class_tests tests/CreateAClassTests.cpp)
    add_executable(gamepad_tests tests/GamepadTests.cpp)
    target_include_directories(gamepad_tests PRIVATE src)
    target_link_libraries(gamepad_tests PRIVATE glfw)
    add_test(NAME gamepad_tests COMMAND gamepad_tests)
    add_executable(animation_set_tests tests/AnimationSetTests.cpp)
    add_executable(trail_geometry_tests tests/TrailGeometryTests.cpp)
    target_include_directories(trail_geometry_tests PRIVATE src)
    add_test(NAME trail_geometry_tests COMMAND trail_geometry_tests)
    add_executable(workspace_mode_tests tests/WorkspaceModeTests.cpp)
    target_include_directories(workspace_mode_tests PRIVATE src)
    add_test(NAME workspace_mode_tests COMMAND workspace_mode_tests)
    add_executable(native_viewhand_skinning_tests tests/NativeViewhandSkinningTests.cpp)
    target_include_directories(native_viewhand_skinning_tests PRIVATE src)
    add_test(NAME native_viewhand_skinning_tests COMMAND native_viewhand_skinning_tests)
    target_include_directories(animation_set_tests PRIVATE src)
    add_test(NAME animation_set_tests COMMAND animation_set_tests)
    target_link_libraries(create_a_class_tests PRIVATE cast_scene cast_weapon cast_assets)
    add_test(NAME create_a_class_tests COMMAND create_a_class_tests)
    add_executable(cast_catalog_report src/tools/CastCatalogReport.cpp)
    target_link_libraries(cast_catalog_report PRIVATE cast_scene)
    add_executable(asset_catalog_report src/tools/AssetCatalogReport.cpp)
    target_link_libraries(asset_catalog_report PRIVATE cast_assets)
    add_executable(take_inspect src/tools/TakeInspect.cpp)
    target_link_libraries(take_inspect PRIVATE cast_take cast_scene cast_core)
    add_executable(source2_sleeve_audit EXCLUDE_FROM_ALL tools/Source2SleeveAudit.cpp)
    target_link_libraries(source2_sleeve_audit PRIVATE cast_scene cast_take stage_renderer OpenGL::GL)
    add_executable(source2_export_audit EXCLUDE_FROM_ALL tools/Source2ExportAudit.cpp)
    target_link_libraries(source2_export_audit PRIVATE cast_assets cast_scene cast_core)
    add_executable(source2_body_audit EXCLUDE_FROM_ALL src/tools/Source2BodyAudit.cpp)
    target_link_libraries(source2_body_audit PRIVATE cast_scene cast_core)
endif()
