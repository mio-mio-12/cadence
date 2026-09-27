param(
    [string]$AssetRoot = 'exported_files',
    [string]$OutputRoot = 'diagnostics/imported_view_fit_v230',
    [string]$Executable = 'build-ui/Release/imported_view_fit_audit.exe'
)
$boHands = "$AssetRoot/bo2/models/viewhands/seal6/c_usa_mp_seal6_longsleeve_viewhands/c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast"
$boWeapon = "$AssetRoot/bo2/models/weapons/view/assault rifles/t6_wpn_ar_an94_view/t6_wpn_ar_an94_view_LOD0.cast"
$boIdle = "$AssetRoot/bo2/animations/viewmodel/an94/viewmodel_an94_idle.cast"
$cases = @(
    @('eldewrito', 'eldewrito/models/assault_rifle_3467_fp_0_20976_weapon.cast', 'eldewrito/models/assault_rifle_3467_fp_0_20976_hands.cast', 'eldewrito/animations/assault_rifle_3467_fp_0_20976/assault_rifle_3467_fp_0_20976_first_person_idle_4.cast'),
    @('cs16', 'cs1.6/models/v_deagle/models/weapons/view/viewmodel_pistol_v_deagle.cast', 'cs1.6/models/v_ak47_hands.cast', 'cs1.6/models/v_deagle/animations/viewmodel_pistol_v_deagle_idle.cast'),
    @('cz', 'cz/models/v_deagle/models/weapons/view/viewmodel_pistol_v_deagle.cast', 'cz/models/v_fiveseven_hands.cast', 'cz/models/v_deagle/animations/viewmodel_pistol_v_deagle_idle.cast'),
    @('css', 'css/models/viewmodel_pistol_v_pist_deagle.cast', 'css/models/v_pist_deagle_hands.cast', 'css/animations/viewmodel_pistol_v_pist_deagle_idle.cast'),
    @('csnz', 'csnz/models/viewmodel_rifle_v_ak47_hq.cast', 'csnz/models/v_ak47_hq_hands.cast', 'csnz/animations/viewmodel_rifle_v_ak47_hq_idle.cast'),
    @('cso2', 'cso2/models/viewmodel_knife_v_knife.cast', 'cso2/models/v_knife_hand_707.cast', 'cso2/animations/viewmodel_knife_v_knife_idle.cast')
)
$failures = 0
foreach ($case in $cases) {
    & $Executable "$AssetRoot/$($case[1])" "$AssetRoot/$($case[2])" $boHands "$AssetRoot/$($case[3])" "$OutputRoot/$($case[0])_bo2"
    if ($LASTEXITCODE -ne 0) { ++$failures }
    Write-Output "$($case[0]) weapon / BO2 hands: exit $LASTEXITCODE"
    & $Executable $boWeapon $boHands "$AssetRoot/$($case[2])" $boIdle "$OutputRoot/bo2_$($case[0])"
    if ($LASTEXITCODE -ne 0) { ++$failures }
    Write-Output "BO2 weapon / $($case[0]) hands: exit $LASTEXITCODE"
}
Write-Output "Failed cases: $failures / 12"
if ($failures) { exit 1 }
