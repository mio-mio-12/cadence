#include "app/WeaponPickerCache.h"
#include <cassert>
#include <iostream>
int main(){
    assets::Catalog c;cadence::WeaponPickerCache cache;
    const auto add=[&](std::string name,std::string game="bocw_sp"){assets::Asset a;a.name=name;a.game=game;a.role=assets::Role::ViewWeapon;c.entries.push_back(a);};
    add("wpn_t9_ar_damage_nocturne_view_LOD0");add("wpn_t9_ar_damage_view_LOD0");
    assert(cache.isColdWarVariant(c,c.entries[0]));assert(!cache.isColdWarVariant(c,c.entries[1]));
    assert(cache.indexBuilds==1);
    for(int i=0;i<1000;++i)assert(cache.isColdWarVariant(c,c.entries[0]));assert(cache.indexBuilds==1);
    c.entries.erase(c.entries.begin()+1);assert(!cache.isColdWarVariant(c,c.entries[0]));
    add("wpn_t9_ar_damage_aaa_view_LOD0");assert(cache.isColdWarVariant(c,c.entries[0]));assert(!cache.isColdWarVariant(c,c.entries[1]));
    int builds=0;const auto build=[&]{++builds;return std::vector<std::size_t>{0};};
    assert(cache.pool(c,"BO2",2,build)==std::vector<std::size_t>{0});cache.pool(c,"bo2",2,build);assert(builds==1);
    cache.pool(c,"bo2",6,build);assert(builds==2);cache.pool(c,"",2,build);assert(builds==3);
    // An in-place replacement must invalidate even with unchanged pointer/size.
    c.entries[0].name="wpn_t9_ar_damage_view_LOD0";cache.clear();assert(!cache.isColdWarVariant(c,c.entries[0]));assert(cache.isColdWarVariant(c,c.entries[1]));
    cache.pool(c,"bo2",2,build);assert(builds==4);
    add("wpn_t9_ar_damage_view_LOD0","t9");assert(!cache.isColdWarVariant(c,c.entries[2]));
    c.clear();assert(cache.pool(c,"bo2",2,[]{return std::vector<std::size_t>{};}).empty());
    add("magnum_7952_fp_1_20984_weapon","eldewrito");add("magnum_7952_fp_0_20976_weapon","eldewrito");add("magnum_7952_fp_1_20984_dual_weapon","eldewrito");
    assert(cache.isImportedVariant(c,c.entries[0]));assert(!cache.isImportedVariant(c,c.entries[1]));assert(!cache.isImportedVariant(c,c.entries[2]));
    add("magnum_7952_fp_0_20976_dual_weapon","eldewrito");assert(cache.isImportedVariant(c,c.entries[2]));assert(!cache.isImportedVariant(c,c.entries[3]));
    std::cout<<"PASS stable index, base preference, absent base, pool keys, mutation invalidation\n";
}
