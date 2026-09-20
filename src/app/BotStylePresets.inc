struct BotStylePreset {const char* name;const char* description;std::array<float,16> chances;};
constexpr std::array<BotStylePreset,5> botStylePresets{{
 {"Aggressive / stylized","Fast pursuit and jumpshots, with occasional SP acting.",{.12f,.40f,.30f,.04f,.05f,.025f,.35f,.035f,.08f,.90f,.42f,.95f,.025f,.08f,.18f,.56f}},
 {"Movement pressure","Frequent sprinting and combat jumps; minimal stationary acting.",{.03f,.10f,.08f,.01f,.01f,.01f,.15f,.01f,.02f,1.f,.85f,1.f,.01f,.025f,.05f,.56f}},
 {"Tactical cover","More cover and cosmetic throws; fewer jumps and ambient interruptions.",{.15f,.20f,.25f,.06f,.10f,.02f,.20f,.02f,.15f,.55f,.12f,.75f,.04f,.35f,.80f,.65f}},
 {"Cinematic patrol","Slower patrols, varied idle acting, stumbles and brief wounded movement.",{.35f,.40f,.35f,.50f,.45f,.18f,.45f,.25f,.65f,.25f,.08f,.50f,.30f,.20f,.40f,.80f}},
 {"SP showcase","High SP variety for inspection. Valid posture, geometry and completion guards still apply.",{.80f,1.f,1.f,1.f,.65f,.65f,.85f,.85f,.30f,.65f,.25f,.80f,.90f,1.f,1.f,1.f}}
}};
std::array<float*,16> botStyleControls(AppState& a){return {&a.botSpAlertChance,&a.botSpPainChance,&a.botSpCrouchChance,&a.botSpIdleChance,&a.botSpPatrolChance,&a.botSpSlipChance,&a.botSpMovingPainChance,&a.botSpAmbientStumbleChance,&a.botSpPatrolWalkChance,&a.botSpSprintChance,&a.botSpJumpChance,&a.botSpCombatMobilityChance,&a.botSpWoundedChance,&a.botSpGrenadeChance,&a.botSpCoverChance,&a.botSpDeathChance};}
void drawBotStylePresets(AppState& app){
 const auto controls=botStyleControls(app);const BotStylePreset* current=nullptr;
 for(const auto& p:botStylePresets){bool match=true;for(size_t i=0;i<controls.size();++i)match&=std::abs(*controls[i]-p.chances[i])<.0001f;if(match){current=&p;break;}}
 if(ImGui::BeginCombo("SP behavior preset",current?current->name:"Custom")){
  for(const auto& p:botStylePresets)if(ImGui::Selectable(p.name,current==&p)){for(size_t i=0;i<controls.size();++i)*controls[i]=p.chances[i];current=&p;}
  ImGui::EndCombo();
 }
 if(current)ImGui::TextWrapped("%s",current->description);
 uiHelp("Applies live to SP chances only; other bot settings stay unchanged.");
}
