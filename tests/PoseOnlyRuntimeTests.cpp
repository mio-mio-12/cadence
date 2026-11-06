#include "scene/CastScene.h"
#include "scene/ImportedViewRetarget.h"
#include <iostream>
int main(){
    using namespace scene;
    CastScene s;s.skeleton.bones.resize(4);
    auto adapter=std::make_shared<CodmRigAdapter>();adapter->firstBone=1;
    for(int i=0;i<3;++i)adapter->bindings.push_back({0,Mat4::identity()});
    // Native wrist is bone0. Donor forearm and upper arm retain 25/30 lengths.
    adapter->positionLinks={{1,0,0,{}},{2,1,2,{-25,0,0}},{3,2,3,{-30,0,0}}};
    s.codmRigAdapter=adapter;std::vector<Transform> local(4);int failures=0;
    for(int frame=0;frame<=120;++frame){
        local[0].position={float(frame),5,9};local[0].rotation=fromAxisAngle({0,0,1},frame*.05f);
        local[0].scale={2,2,2}; // Native scale must not change donor proportions.
        const auto pose=s.globalPose(local);
        const auto p=[&](int i){return transformPoint(pose[i],{});};
        if(length(p(1)-p(0))>1e-4f||std::abs(length(p(2)-p(1))-25)>1e-4f||std::abs(length(p(3)-p(2))-30)>1e-4f)++failures;
        if(std::abs(length(transformPoint(pose[2],{0,3,0})-p(2))-3)>1e-4f)++failures;
    }
    // Existing non-pose-only adapters retain their old transform behavior.
    s.skeleton.bones.emplace_back();local.emplace_back();local.back().scale={4,4,4};
    const auto extra=s.globalPose(local);
    if(std::abs(extra.back().v[0]-4)>1e-4f)++failures; // Later weapon/helper bones are not donor skin.
    adapter->positionLinks.clear();const auto old=s.globalPose(local);
    if(std::abs(length(transformPoint(old[2],{0,3,0})-transformPoint(old[2],{}))-6)>1e-4f)++failures;
    // Split native CS2 skins must follow the actual twist track, not a
    // reconstructed elbow/wrist interpolation, without changing vertices.
    CastScene driver,skin;driver.sourceModelPath="/exports/cs2/models/ak47_model.cast";skin.sourceModelPath="/exports/cs2/models/gloves/hands.cast";
    Bone twist;twist.name="arm_lower_L_TWIST";twist.restGlobal=Mat4::identity();twist.inverseBind=Mat4::identity();
    driver.skeleton.bones.push_back(twist);driver.skeleton.boneByName[twist.name]=0;
    twist.restGlobal.v[12]=.02f;twist.inverseBind=inverseAffine(twist.restGlobal);skin.skeleton.bones.push_back(twist);skin.skeleton.boneByName[twist.name]=0;
    Mesh mesh;mesh.skinned=true;Vertex vertex;vertex.position={0,1,0};vertex.weights={1,0,0,0};mesh.vertices.push_back(vertex);skin.meshes.push_back(mesh);
    std::string error;if(!imported::fitPoseOnlyViewSkin(driver,skin,error)||!driver.codmRigAdapter)++failures;
    else {std::vector<Transform> locals(driver.skeleton.bones.size());locals[0].rotation=fromAxisAngle({1,0,0},1.f);auto pose=driver.globalPose(locals);const auto& v=driver.meshes.back().vertices[0];const auto actual=transformPoint(pose[v.bones[0]]*driver.skeleton.bones[v.bones[0]].inverseBind,v.position);const auto expected=transformPoint(rotation(locals[0].rotation),vertex.position);if(length(actual-expected)>1e-4f||length(v.position-vertex.position)>1e-6f||v.weights!=vertex.weights)++failures;}
    CastScene rollScene;rollScene.skeleton.bones.resize(3);auto rollAdapter=std::make_shared<CodmRigAdapter>();rollAdapter->firstBone=2;
    CodmRigBinding rollBinding;rollBinding.source=0;rollBinding.rollSource=1;rollBinding.rollWeight=1;rollBinding.rollAxis={1,0,0};rollAdapter->bindings.push_back(rollBinding);rollScene.codmRigAdapter=rollAdapter;
    std::vector<Transform> rollLocal(3);rollLocal[1].rotation=multiply(fromAxisAngle({0,0,1},.7f),fromAxisAngle({1,0,0},.9f));const auto rollPose=rollScene.globalPose(rollLocal);
    if(length(transformPoint(rollPose[2],{1,0,0})-Vec3{1,0,0})>1e-4f||length(transformPoint(rollPose[2],{0,1,0})-transformPoint(rotation(fromAxisAngle({1,0,0},.9f)),{0,1,0}))>1e-4f)++failures;

    // A flattened track can animate its next joint independently of rotation.
    // Follow the posed direction while retaining the donor segment length.
    CastScene finger;finger.skeleton.bones.resize(4);
    auto fingerAdapter=std::make_shared<CodmRigAdapter>();
    fingerAdapter->firstBone=2;fingerAdapter->bindings={{0},{1}};
    fingerAdapter->positionLinks={{2,0,2,{}},{3,2,2,{2,0,0},0,1}};
    finger.codmRigAdapter=fingerAdapter;
    std::vector<Transform> fingerLocal(4);
    fingerLocal[0].position={3,5,7};fingerLocal[1].position={3,8,7};
    const auto fingerPose=finger.globalPose(fingerLocal);
    if(length(transformPoint(fingerPose[3],{})-Vec3{3,7,7})>1e-4f||
       length(transformPoint(fingerPose[2],{2,0,0})-Vec3{3,7,7})>1e-4f)++failures;
    std::cout<<"Pose-only runtime: "<<failures<<" failures\n";return failures?1:0;
}
