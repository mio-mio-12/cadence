#include "scene/PointBlankNative.h"
#include "scene/PointBlankWorld.h"
#include "scene/GripLandmarks.h"
#include <fstream>
#include "third_party/nlohmann_json.hpp"
using nlohmann::json;
int main(){
 const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files/pointblank";
 json result;
 auto dump=[&](const std::string& label,const scene::CastScene& s,const std::vector<scene::Mat4>& pose){for(size_t i=0;i<s.skeleton.bones.size();++i)result[label][s.skeleton.bones[i].name]=pose[i].v;};
 auto body=scene::buildScene(cast::Document::load(root/"models/playermode_SWAT_Male_fb.cast"),false);std::vector<scene::Mat4> bind;for(auto& b:body.skeleton.bones)bind.push_back(b.restGlobal);dump("body",body,bind);
 std::ifstream input("cadence weapon calibrator reference/latest.json");json refs;input>>refs;
 for(auto& c:refs["cases"]){auto target=scene::buildScene(cast::Document::load(c["body_cast"].get<std::string>()),false);std::vector<scene::Mat4> tb;for(auto&b:target.skeleton.bones)tb.push_back(b.restGlobal);dump("target",target,tb);scene::CastScene s;std::string error;if(!scene::pointblank::assemble(cast::Document::load(c["weapon_cast"].get<std::string>()),cast::Document::load(root/"models/viewmodel_SWAT_Male_hands.cast"),s,error))return 2;
 const std::string id=c["case"],stem=std::filesystem::path(c["weapon_cast"].get<std::string>()).stem().string();const auto idle=root/"animations/viewmodel"/(id=="pistol_python"?"coltpython":"ak-47")/(stem+"_AttackIdle.cast");scene::appendAnimations(cast::Document::load(idle),s);const auto pose=s.samplePose(0,0);dump(id,s,pose);
 std::map<std::string,scene::Mat4> frozen;for(auto&j:c["frozen_pose"]){scene::Mat4 m;for(int k=0;k<16;++k)m.v[k]=j["global_matrix_column_major"][k];frozen[j["name"].get<std::string>()]=m;}
 auto sw=scene::inverseAffine(pose[s.skeleton.boneByName.at("R Hand")]),tw=scene::inverseAffine(frozen.at("b_RightHand"));std::vector<scene::Vec3> from,to;int f=0;for(auto name:{"Thumb","Index","Middle","Ring","Little"}){for(int l=1;l<=2;++l){from.push_back(scene::transformPoint(sw*pose[s.skeleton.boneByName.at("R "+std::string(name)+std::to_string(l))],{}));to.push_back(scene::transformPoint(tw*frozen.at("b_RightFinger"+std::to_string(f)+(l==1?"":"1")),{}));}++f;}
 auto fit=scene::fitGripLandmarks(from,to);if(!fit)return 3;
 scene::Vec3 p,scale;scene::Quat q,t;scene::decomposeAffine(body.skeleton.bones[body.skeleton.boneByName.at("R Hand")].restGlobal,p,q,scale);scene::decomposeAffine(target.skeleton.bones[target.skeleton.boneByName.at("b_RightHand")].restGlobal,p,t,scale);
 const auto oldBasis=scene::trs({},scene::multiply(scene::Quat{-t.x,-t.y,-t.z,t.w},q),{1,1,1});const auto delta=*fit*scene::pointBlankGripSurfaceCalibration(id=="pistol_python")*scene::inverseAffine(oldBasis);float maxError=0;for(int k=0;k<16;++k)maxError=std::max(maxError,std::abs(delta.v[k]-c["adjusted"]["matrix_column_major"][k].get<float>()));result[id+"_accepted_reference_max_matrix_error"]=maxError;if(maxError>.002f)return 4;
 }
 std::filesystem::create_directories("diagnostics/v158-world-grips");std::ofstream("diagnostics/v158-world-grips/landmarks.json")<<result.dump(2);return 0;
}
