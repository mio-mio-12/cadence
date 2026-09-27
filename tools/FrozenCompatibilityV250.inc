// Frozen v250 compatibility implementation for independent regression comparison.
// Keep unchanged when editing the production implementation.
namespace frozen_catalog {
using assets::Role;
std::string lower(std::string_view input){std::string value(input);std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
bool contains(std::string_view value,std::string_view part){return value.find(part)!=std::string_view::npos;}
std::vector<std::string> tokens(std::string_view input){std::vector<std::string> result;std::string token;
    for(const unsigned char c:input){if(std::isalnum(c))token.push_back(static_cast<char>(std::tolower(c)));else if(!token.empty()){result.push_back(std::move(token));token.clear();}}
    if(!token.empty())result.push_back(std::move(token));return result;}
std::vector<std::string> compatibilityKeys(std::string_view input,Role role){auto values=tokens(input);std::vector<std::string> result;
    const std::unordered_set<std::string> common{"t5","t6","t7","iw3","iw4","iw5","iw6","iw7","s1","s2","cod4","mw2","mw3","ghosts","cs2","model","models","wpn","weapon","viewmodel","vm","va","attach","view","world","lod0","lod1","lod2","lod3","base","standard","standard2","operator1","operator2","pro","pro2","royal","atlas","black","gold","mobility","damage","handling","accuracy","range","fire","rate","default","stnd","op01","op02","npc","wm","brock","bshdwl","bwmrpt","cmdtgr","stagger","autumn","blue","choco","hex","marine","multi","red","snake","snow","winter","arctic","woodland"};
    const std::unordered_set<std::string> attachmentTypes{"silencer","silencer1","silencer2","silencer3","silencer4","fastmag","mag","gl","grip","optic","acog","combo","dualband","mount","holo","ads","mms","rangefinder","reflex","rmr","specter","vzoom","bcpu","dbal","wlp","speedloader"};
    for(auto value:values){if(common.contains(value))continue;
        if((role==Role::WorldAttachment||role==Role::ViewAttachment)&&attachmentTypes.contains(value))continue;
        if(value=="scar")value="scarh";else if(value=="scorpion"||value=="evoskorpion")value="skorpion";
        else if(value=="saiga"){ if(std::find(result.begin(),result.end(),"saiga")==result.end())result.push_back("saiga"); value="saiga12"; }
        else if(value=="saiga12"){ if(std::find(result.begin(),result.end(),"saiga")==result.end())result.push_back("saiga"); }
        else if(value=="mk14"||value=="m14"||value=="m14ebr"||value=="mk14ebr"||value=="m14sd"){ if(std::find(result.begin(),result.end(),"m14")==result.end())result.push_back("m14"); value="m14ebr"; }
        else if(value=="arx"||value=="arx160"||value=="arx_160"){ if(std::find(result.begin(),result.end(),"arx")==result.end())result.push_back("arx"); value="arx160"; }
        else if(value=="honeybadger"||value=="honey_badger")value="honeybadger";
        else if(value=="remingtonr5"||value=="remington_r5"||value=="r5"||value=="r5rgp"||value=="remington_r5rgp"||value=="rm22"||value=="rm_22_ar"||value=="rm_22"){ if(std::find(result.begin(),result.end(),"r5rgp")==result.end())result.push_back("r5rgp"); value="remingtonr5"; }
        else if(value=="remington"||value=="remington700"||value=="r700")value="remington700";
        else if(value=="sc2010")value="sc2010";
        else if(value=="m27"||value=="m27_iar"||value=="m27iar"){ if(std::find(result.begin(),result.end(),"m27")==result.end())result.push_back("m27"); value="m27iar"; }
        else if(value=="cbj"||value=="cbjms"||value=="cbj_ms"){ if(std::find(result.begin(),result.end(),"cbj")==result.end())result.push_back("cbj"); value="cbjms"; }
        else if(value=="dragunovsvu"||value=="dragunov_svu"){ if(std::find(result.begin(),result.end(),"dragunov")==result.end())result.push_back("dragunov"); value="dragunovsvu"; }
        else if(value=="fad"||value=="fads"){ if(std::find(result.begin(),result.end(),"fad")==result.end())result.push_back("fad"); value="fads"; }
        else if(value=="mts"||value=="mts255"||value=="mts_255"){ if(std::find(result.begin(),result.end(),"mts")==result.end())result.push_back("mts"); value="mts255"; }
        else if(value=="panzerfaust3"||value=="panzerfaust"){ if(std::find(result.begin(),result.end(),"panzerfaust")==result.end())result.push_back("panzerfaust"); value="panzerfaust"; }
        else if(value=="ak47"||value=="ak_47"||value=="ak"||value=="ak47sd")value="ak47";
        else if(value=="ak74u"||value=="ak74usd"){ if(std::find(result.begin(),result.end(),"ak74usd")==result.end())result.push_back("ak74usd"); value="ak74u"; }
        else if(value=="g36c"||value=="g36"||value=="g36csd"){ if(std::find(result.begin(),result.end(),"g36c")==result.end())result.push_back("g36c"); value="g36"; }
        else if(value=="g36cm203"||value=="g36m203")value="g36m203";
        else if(value=="m4a4"||value=="m4a1"||value=="m4"||value=="m4sd")value="m4a4";
        else if(value=="g3sd"||value=="g3")value="g3";
        else if(value=="m16sd"||value=="m16")value="m16";
        else if(value=="mp5sd"||value=="mp5")value="mp5";
        else if(value=="miniuzi"||value=="mini_uzi")value="miniuzi";
        else if(value=="saw"||value=="m249saw"||value=="m249")value="m249";
        else if(value=="rpg7"||value=="rpg"){ if(std::find(result.begin(),result.end(),"rpg7")==result.end())result.push_back("rpg7"); value="rpg"; }
        else if(value=="skorpionsd"||value=="skorpion")value="skorpion";
        else if(value=="winchester1200"||value=="winchest1200"||value=="winchester"||value=="w1200"){ if(std::find(result.begin(),result.end(),"winchester")==result.end())result.push_back("winchester"); value="winchester1200"; }
        else if(value=="colt1911"||value=="colt45"||value=="m1911"){ if(std::find(result.begin(),result.end(),"colt45")==result.end())result.push_back("colt45"); value="colt1911"; }
        else if(value=="benelli"||value=="benellim4"||value=="super90"||value=="super_90"){ if(std::find(result.begin(),result.end(),"benellim4")==result.end())result.push_back("benellim4"); value="benelli"; }
        else if(value=="karambit"||value=="knife_karambit")value="karambit";
        else if(value=="butterfly"||value=="knife_butterfly")value="butterfly";
        else if(value=="bayonet"||value=="knife_bayonet")value="bayonet";
        else if(value=="cz805"||value=="cz_805_bren"||value=="bren"||value=="sa805")value="sa805";
        else if(value=="masada")value="acr";
        else if(value=="cheytac")value="intervention";
        else if(value=="rottweil72")value="olympia";
        else if(value=="ithaca"||value=="stakeout")value="ithaca37";
        else if(value=="insas")value="msmc";
        else if(value=="qcw05"||value=="chicom")value="chicom";
        else if(value=="kard"){ if(std::find(result.begin(),result.end(),"kard")==result.end())result.push_back("kard"); value="kap40"; }
        else if(value=="kap40"){ if(std::find(result.begin(),result.end(),"kard")==result.end())result.push_back("kard"); }
        else if(value=="fnp45"){ if(std::find(result.begin(),result.end(),"fnp45")==result.end())result.push_back("fnp45"); value="tac45"; }
        else if(value=="tac45"){ if(std::find(result.begin(),result.end(),"fnp45")==result.end())result.push_back("fnp45"); }
        else if(value=="mk48"){ if(std::find(result.begin(),result.end(),"mk48")==result.end())result.push_back("mk48"); value="mark48"; }
        else if(value=="mark48"){ if(std::find(result.begin(),result.end(),"mk48")==result.end())result.push_back("mk48"); }
        else if(value=="x95l"){ if(std::find(result.begin(),result.end(),"x95l")==result.end())result.push_back("x95l"); value="tavor"; }
        else if(value=="tavor"){ if(std::find(result.begin(),result.end(),"x95l")==result.end())result.push_back("x95l"); }
        else if(value=="ballistic_knife"||value=="ballisticknife"){ if(std::find(result.begin(),result.end(),"ballistic_knife")==result.end())result.push_back("ballistic_knife"); value="ballistic_knife_t6"; }
        else if(value=="crossbow"){ if(std::find(result.begin(),result.end(),"crossbow")==result.end())result.push_back("crossbow"); value="crossbow_t6"; }
        else if(value=="c4"){ if(std::find(result.begin(),result.end(),"c4")==result.end())result.push_back("c4"); value="c4_t6"; }
        else if(value=="pda"){ if(std::find(result.begin(),result.end(),"pda")==result.end())result.push_back("pda"); value="pda_hacker"; }
        else if(value=="shield"||value=="riotshield"){ if(std::find(result.begin(),result.end(),"shield")==result.end())result.push_back("shield"); value="riotshield"; }
        else if(value=="tac_insert"||value=="tacinsert"){ if(std::find(result.begin(),result.end(),"tac_insert")==result.end())result.push_back("tac_insert"); value="tac_insert_t6"; }
        else if(value=="taser_mine"||value=="tasermine"||value=="tazor_spike"){ if(std::find(result.begin(),result.end(),"taser_mine")==result.end())result.push_back("taser_mine"); value="tazor_spike"; }
        else if(value=="beretta93r"||value=="b2023r"||value=="b23r")value="b23r";
        else if(value=="fnfiveseven"||value=="fiveseven"||value=="fn57"){ if(std::find(result.begin(),result.end(),"fn57")==result.end())result.push_back("fn57"); if(std::find(result.begin(),result.end(),"fnfiveseven")==result.end())result.push_back("fnfiveseven"); value="fiveseven"; }
        else if(value=="coltanaconda"||value=="magum"||value=="magnum")value="magnum";
        else if(value=="l96a1"||value=="l96"){ if(std::find(result.begin(),result.end(),"l96")==result.end())result.push_back("l96"); value="l96a1"; }
        else if(value=="m82"||value=="barrett"||value=="barrett50cal"){ if(std::find(result.begin(),result.end(),"barrett")==result.end())result.push_back("barrett"); value="m82"; }
        else if(value=="sa80"||value=="sa80lmg"){ if(std::find(result.begin(),result.end(),"sa80lmg")==result.end())result.push_back("sa80lmg"); value="sa80"; }
        else if(value=="model1887"||value=="1887"){ if(std::find(result.begin(),result.end(),"model1887")==result.end())result.push_back("model1887"); value="1887"; }
        else if(value=="lightstick"||value=="light_stick")value="lightstick";
        else if(value=="m8garandscope"||value=="garand"||value=="m1garand"||value=="m1"){ if(std::find(result.begin(),result.end(),"m1")==result.end())result.push_back("m1"); value="garand"; }
        else if(value=="chainsaw"||value=="kac_chainsaw"||value=="chain_saw")value="chainsaw";
        else if(value=="fabarm_fp6"||value=="fp6")value="fp6";
        else if(value=="maul")value="bulldog";
        else if(value=="g28")value="mr28";
        else if(value=="gm6")value="lynx";
        else if(value=="l115a3")value="l115";
        else if(value=="vbr_pdw"||value=="vbr")value="pdw";
        else if(value=="evopro")value="evo";
        else if(value=="kriss")value="vector";
        else if(value=="crdb")value="karma45";
        else if(value=="chargeshot")value="ebr800";
        else if(value=="devastator")value="mauler";
        else if(value=="dbl50")value="oni";
        else if(value=="e_ak47")value="volk";
        else if(value=="cq300"||value=="cq_300")value="cq300";
        else if(value=="kbr32"||value=="kbar")value="kbar32";
        else if(value=="rprevo")value="rpr";
        else if(value=="spartansa3")value="spartan";
        else if(value=="stingerm7")value="stinger";
        else if(value=="atlas20mm")value="atlas20";
        else if(value=="tar21"||value=="mtarx"||value=="x95l")value="mtar";
        else if(value=="sig556")value="sig556";
        else if(value=="saritch")value="saritch";
        else if(value=="qbb95")value="qbb95";
        else if(value=="srm1216")value="m1216";
        else if(value=="870mcs")value="870mcs";
        else if(value=="ump45")value="ump";
        else if(value=="ak12")value="ak12";
        else if(value=="usr")value="usr";
        if(std::find(result.begin(),result.end(),value)==result.end())result.push_back(value);
    }
    const auto name=lower(input);
    if(contains(name,"mk14")||contains(name,"m14ebr")){
        if(std::find(result.begin(),result.end(),"m14ebr")==result.end())result.push_back("m14ebr");
        if(std::find(result.begin(),result.end(),"mk14")==result.end())result.push_back("mk14");
    }
    if(role==Role::WorldAttachment||role==Role::ViewAttachment){
        if(contains(name,"_attach_gl_")&&std::find(result.begin(),result.end(),"ar")==result.end())result.push_back("ar");
        if(contains(name,"_attach_speedloader")&&std::find(result.begin(),result.end(),"judge")==result.end())result.push_back("judge");
    }
    return result;
}



}
