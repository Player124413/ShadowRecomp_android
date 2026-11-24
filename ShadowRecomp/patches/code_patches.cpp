#include <kernel/function.h>
#include <input/padstate.h>
#include <user/config.h>
#include <patches/patch_utils.h>

void CodeValuePatches()
{
    auto base = g_memory.base;
    if (Config::SelectModeEnableLastStoryDevilDoomSlots || Config::ReloadedMod)
    {
        X86_STORE_U32(0x478EF0, 0x1B); // Last Way below Final Haunt
        X86_STORE_U32(0x478F90, 0x2C); // Devil Doom right of Last Way
    }
    if (Config::AllPartnersCanHomingAttack)
    {
        for (uint32_t character = 2; character <= 10; character++)
        {
            X86_STORE_U8(0x449D68 + character * 0x2C, 1); // homing attack
        }
        X86_STORE_U8(0x449EF5, 1); // maria jumpdash
    }
}

// Vacuum gives score + hero points on sucking up fire
// Fix the missing ScoreCommand if vacuum picks up fire (debatable if this was a bug or not, so making this a cheat)
// Fire VacuumCommand handler @ 0x1AA820
// Fire attack callback @ 0x19B550
X86_FUNC_IMPL(__imp__sub_5C4D0); // ScoreCommand
void FireVacuumCallbackMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    if (!Config::VacuumGivePointsOnFireSuck)
    {
        return;
    }

    // ebp = GadgetCharInfo (object being sucked)
    // ebp+0x50 = GadgetID, 0x1A = Fire
    // ebp+0x64 = CharCommandUnit, handler instance is 0x8 inside CharCommandUnit
    // edi = Vacuum weapon instance (0xC = player who is holding it)
    if (ctx.ebp.u32 == 0 || X86_LOAD_U32(ctx.ebp.u32 + 0x50) != 0x1A)
    {
        // skip if not fire
        return;
    }

    auto cmdUnit = X86_LOAD_U32(ctx.ebp.u32 + 0x64);
    if (cmdUnit == 0) // command null check (matching OG)
    {
        return;
    }

    auto node = X86_LOAD_U32(cmdUnit + 8);
    if (node == 0 || X86_LOAD_U32(node) != 0x1AA820) // check for Fire/PoisonGas handler
    {
        return;
    }

    // 0x466F6C = Fire & PoisonGas vtable (shared, but PoisonGas has no score attribute)
    // fire+0x50 = state
    auto fire = X86_LOAD_U32(node + 4);
    if (X86_LOAD_U32(fire) != 0x466F6C || X86_LOAD_U32(fire + 0x50) != 1)
    {
        return;
    }

    // safety check if the vac command drops
    if (X86_LOAD_U32(ctx.edi.u32 + 0xC) == 0)
    {
        return;
    }

    ctx.esp.u32 -= 4;
    X86_STORE_U32(ctx.esp.u32, 0x1B); // score callback entry
    ctx.esp.u32 -= 4;
    X86_STORE_U32(ctx.esp.u32, ctx.edi.u32 + 0xC); // player instance
    ctx.esp.u32 -= 4;
    __imp__sub_5C4D0(ctx, base);
    ctx.esp.u32 += 8;
}

void ToggleExpertMode()
{
    auto base = g_memory.base;
    X86_STORE_U8(0x54DE55, 1);
}

// Expert Select
void ExpertSelectMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    const bool enabled = Config::SelectModeExpertStages || Config::ReloadedMod;
    if (enabled && PJSPeripheral::IsHeld(0, PJSPeripheral::BTN_X))
    {
        ToggleExpertMode();
    }
}

bool ExpertSelectNoSaveMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return X86_LOAD_U32(0x54DDFC) == 0;
}

// Hold LT to force JumpDash standalone code (also enabled with Reloaded)
// CreateBehavior (all player actions)
//
// Table of all PlayerBehavior @ 0x4EEE58
// 0x6 = JumpDash
// 0x8 = HomingAttack
X86_FUNC_IMPL(__imp__sub_977D0);
X86_OVERRIDE_FUNC(sub_977D0)
{
    // esp+8 = BehaviorId
    // ecx+4 = PlayerStruct targetted by Behavior
    const bool forceJumpDash = Config::HoldLTForceJumpDash || Config::ReloadedMod;
    if (forceJumpDash && X86_LOAD_U32(ctx.esp.u32 + 8) == 8) // HomingAttack
    {
        const uint32_t port = GetPlayerPort(X86_LOAD_U32(ctx.ecx.u32 + 4));
        if (port < 4 && PJSPeripheral::IsHeld(port, PJSPeripheral::BTN_LEFT_TRIG))
        {
            X86_STORE_U32(ctx.esp.u32 + 8, 6); // Change target Behavior to JumpDash
        }
    }
    __imp__sub_977D0(ctx, base);
}

// Chaos Gauge Fill Cheat
// RB + R (or F7) fills the hero gauge, RB + L (or F6) fills the dark one.
static constexpr uint32_t HeroGaugeOffset = 0x70C;
static constexpr uint32_t DarkGaugeOffset = 0x718;

extern std::atomic<bool> g_chaosControlHoldCheat;
extern std::atomic<bool> g_chaosBlastHoldCheat;

static bool s_heroComboHeld[4] = {};
static bool s_darkComboHeld[4] = {};

static void FillChaosGauge(uint32_t gauge, uint32_t player)
{
    auto base = g_memory.base;
    uint32_t gameState = X86_LOAD_U32(0x54963C);
    if (!gameState)
        return;

    uint32_t max = X86_LOAD_U32(gameState + gauge + 4);

    if ((int32_t)max <= 0)
        return;

    X86_STORE_U32(gameState + gauge, max);
}

static void HandleChaosGaugeCheat(X86Context& ctx, uint8_t* base)
{
    const bool isCCCheat = Config::FillChaosControlGaugeOnInput;
    const bool isCBCheat = Config::FillChaosBlastGaugeOnInput;

    if (isCCCheat && g_chaosControlHoldCheat)
        FillChaosGauge(HeroGaugeOffset, 0);
    if (isCBCheat && g_chaosBlastHoldCheat)
        FillChaosGauge(DarkGaugeOffset, 0);

    if (isCCCheat || isCBCheat)
    {
        uint32_t player = X86_LOAD_U32(ctx.ecx.u32 + 0x28);
        uint32_t pad = GetPlayerPort(player);
        if (pad >= 4)
        {
            return;
        }

        using namespace PJSPeripheral;
        bool hero = IsHeld(pad, BTN_BLACK) && IsHeld(pad, BTN_RIGHT_TRIG);
        bool dark = IsHeld(pad, BTN_BLACK) && IsHeld(pad, BTN_LEFT_TRIG);

        if (player && hero && !s_heroComboHeld[pad] && isCCCheat)
            FillChaosGauge(HeroGaugeOffset, player);
        else if (player && dark && !s_darkComboHeld[pad] && isCBCheat)
            FillChaosGauge(DarkGaugeOffset, player);

        s_heroComboHeld[pad] = hero;
        s_darkComboHeld[pad] = dark;
    }
}

// ShadowExecuter::Exec
X86_FUNC_IMPL(__imp__sub_98670);
X86_OVERRIDE_FUNC(sub_98670)
{
    HandleChaosGaugeCheat(ctx, base);
    __imp__sub_98670(ctx, base);
}

// Infinite Ammo Cheat
// If the game is removing an ammo, swap it to 0 so shot cost nothing
X86_FUNC_IMPL(__imp__sub_9FC00);
X86_OVERRIDE_FUNC(sub_9FC00)
{
    if (Config::InfiniteAmmo && (int32_t)X86_LOAD_U32(ctx.esp.u32 + 4) < 0)
    {
        X86_STORE_U32(ctx.esp.u32 + 4, 0);
    }
    __imp__sub_9FC00(ctx, base);
}

// Vacuum Any Enemy Cheat
// Both Vacuum weapons look for the collisions flag 0x80 to detect if they can work on the current enemy
// The game don't use it on some enemy like the BkNinja and Big Foot, so we add it manually 
void VacuumAnyEnemyColliMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    if (Config::VacuumAnyEnemy)
        ctx.ecx.u32 |= 0x80;
}

// Both BkWingLarge and BkChaos can resist the vacuum command, force it to always work.
bool VacuumAnyEnemySkipResistMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return Config::VacuumAnyEnemy;
}

// BkNinja puts a head aim callback on one bone with the enemy as data, which makes the game reads freed memory if you suck it, this eventually corrupts and crash the game.
// We can fix that by clearing all callback nodes.
static void ClearNodeCallbacks(uint8_t* base, uint32_t node, uint32_t depth)
{
    while (node != 0 && depth < 64)
    {
        X86_STORE_U32(node + 0x1C, 0); // callback
        X86_STORE_U32(node + 0x20, 0); // callback data
        ClearNodeCallbacks(base, X86_LOAD_U32(node + 0x24), depth + 1); // child
        node = X86_LOAD_U32(node + 0x28); // sibling
    }
}

// CEnemyVacuumObject ctor
X86_FUNC_IMPL(__imp__sub_CEED0);
X86_OVERRIDE_FUNC(sub_CEED0)
{
    uint32_t vacuumObject = ctx.ecx.u32;
    __imp__sub_CEED0(ctx, base);

    if (!Config::VacuumAnyEnemy)
        return;

    uint32_t model = X86_LOAD_U32(vacuumObject + 0x34);
    if (model == 0 || X86_LOAD_U32(model) == 0)
        return;

    uint32_t hierarchy = X86_LOAD_U32(X86_LOAD_U32(model) + 8);
    if (hierarchy != 0)
        ClearNodeCallbacks(base, X86_LOAD_U32(hierarchy + 0xC), 0);
}

// XboxEarlyTargetBehavior = true to restore Oct 16 build behavior (target BkWorms out of bounds, BkNinja in warping state)
// Matches the GC enemy hidden state handle. After digging more into it this seems like an accidental change in Oct 16 builds.
bool EnemyColliderXboxGCStateMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return !Config::XboxEarlyTargetBehavior;
}

// copies sub_CDC20's skipped block from the missing init for enemy AI state
X86_FUNC_IMPL(__imp__sub_CE6C0);
X86_FUNC_IMPL(__imp__sub_12350);
static void CreateEnemyColliders(const X86Context& ctx, uint8_t* base, uint32_t status)
{
    const uint32_t enemy = X86_LOAD_U32(status + X86_LOAD_U32(X86_LOAD_U32(status + 0x4) + 0x4) + 0x4);
    const uint32_t param = X86_LOAD_U32(enemy + 0x4);
    const uint32_t colliInfo = GuestToHostFunction<uint32_t, CallConvention::Thiscall>(ctx, X86_LOAD_U32(X86_LOAD_U32(param) + 0x6C), param);
    X86Context call = ctx;
    call.esp.u32 -= 8;
    const uint32_t colliders = call.esp.u32;
    X86_PUSH(call, base, colliInfo);
    X86_PUSH(call, base, enemy);
    X86_PUSH(call, base, colliders);
    call.esp.u32 -= 4;
    __imp__sub_CE6C0(call, base);

    // GC inits these at 0,0,0 need to match otherwise WaitAppear enemies can be homing attacked
    const uint32_t colliderState = X86_LOAD_U32(colliders);
    for (uint32_t i = 0; colliderState != 0 && i < 2; i++)
    {
        const uint32_t collider = X86_LOAD_U32(colliderState + 0x8 + i * 8);
        if (collider != 0)
        {
            X86_STORE_U32(collider + 0x8, 0);
            X86_STORE_U32(collider + 0xC, 0);
            X86_STORE_U32(collider + 0x10, 0);
        }
    }

    const uint32_t oldCount = X86_LOAD_U32(status + 0x68);
    X86_STORE_U32(status + 0x64, colliderState);
    X86_STORE_U32(status + 0x68, X86_LOAD_U32(colliders + 0x4));
    if (oldCount != 0)
    {
        GuestToHostFunction<void, CallConvention::Thiscall>(ctx, __imp__sub_12350, oldCount);
    }
}

// EnemyStatusCommon slot 0 (Oct 12 and earlier build set the colliders on init)
X86_FUNC_IMPL(__imp__sub_CCC90);
X86_OVERRIDE_FUNC(sub_CCC90)
{
    const uint32_t status = ctx.ecx.u32;
    __imp__sub_CCC90(ctx, base);
    if (!Config::XboxEarlyTargetBehavior)
    {
        CreateEnemyColliders(ctx, base, status);
    }
}

void PlayAsBattleCharactersMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    int32_t character = X86_LOAD_U32(0x54DE6C); // P1 battle character
    if (Config::ExtendedMultiplayerP1CharId >= 0)
    {
        character = Config::ExtendedMultiplayerP1CharId;
    }
    const bool enabled = Config::PlayAsBattleCharacters || Config::ReloadedMod;
    if (enabled && X86_LOAD_U32(0x54DDD8) != 710 && character > 0xE && character <= 0x13)
    {
        X86_STORE_U32(ctx.esp.u32, character);
    }
}

static bool IsShadowAndroidWithWeapons(uint8_t* base, uint32_t player)
{
    if (!Config::ShadowAndroidsNoEmbeddedWeapons)
    {
        return false;
    }
    const uint32_t character = X86_LOAD_U32(player + 0x64);
    return character >= 0x10 && character <= 0x13;
}

// skip embedded gun/bazooka flag
bool ShadowAndroidsSkipEmbeddedWeaponMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return IsShadowAndroidWithWeapons(base, ctx.edi.u32);
}

// vehicle driving over weapons unrestrict
bool ShadowAndroidsVehicleWeaponMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return IsShadowAndroidWithWeapons(base, ctx.eax.u32);
}

bool SelectMenuExpansionMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return (Config::SelectModeEnableLastStoryDevilDoomSlots || Config::ReloadedMod) && ctx.esi.u32 == 5 && ctx.edi.u32 == 5;
}

bool SkipLoadingAnyEventsMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    if (!Config::SkipLoadingAnyEvents)
    {
        return false;
    }
    ctx.esp.u32 += 8;
    return true;
}

// Single Frame Invuln/Recovery
bool SingleFrameInvulnMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return Config::SingleFrameInvulnRecovery;
}

// make state 1 skip the flag 0x25 check
bool DamageSkipFlagMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    return Config::SingleFrameInvulnRecovery;
}

// jump to recovery branch
bool DamageImmediateRecoverMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
    if (!Config::SingleFrameInvulnRecovery)
    {
        return false;
    }
    X86_STORE_U32(ctx.esi.u32 + 0x1C, 2);
    X86_STORE_U32(ctx.esi.u32 + 0x20, 1);
    return true;
}
