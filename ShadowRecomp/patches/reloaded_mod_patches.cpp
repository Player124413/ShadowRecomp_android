#include <kernel/function.h>
#include <input/padstate.h>
#include <user/config.h>
#include <patches/patch_utils.h>


// Player::Behaviors @ 0x4EEE58
enum PlayerBehaviorId : uint32_t
{
	BEHAVIOR_FALL = 0x2,
	BEHAVIOR_JUMP_DASH = 0x6,
	BEHAVIOR_HOMING_ATTACK = 0x8,
	BEHAVIOR_HOMING_JUMP = 0x9,
	BEHAVIOR_SPIN_DASH = 0x12,
	BEHAVIOR_LAUNCH = 0x17,
};

static constexpr uint32_t g_currentStageId = 0x54DDD8;
static constexpr uint32_t LAUNCH_FLAG_NO_BLUR = 0x80000000; // disables CommonEffect 'BLUR' id 0x5C

// Inlined Homing Attack Speed 1000f -> 2000f [Reloaded Phys]
void HomingAttackSpeedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod ? 2000.0f : 1000.0f);
}

// Circuit Speed Control
// Electric::Apply @ sub_6F9E0
// Spline::Circuit read speed @ 0x4EC5E8
void CircuitSpeedControlMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		ctx.xmm[0].f32[0] = X86_LOAD_F32(0x4EC5E8);
		return;
	}

	// ecx = Behavior::ElectricCircuit
	// ecx+4 = player instance
	const uint32_t port = GetPlayerPort(X86_LOAD_U32(ctx.ecx.u32 + 4));
	float speed = 800.0f;
	if (port < 4 && PJSPeripheral::IsHeld(port, PJSPeripheral::BTN_A))
	{
		speed = 1600.0f;
	}
	else if (port < 4 && PJSPeripheral::IsHeld(port, PJSPeripheral::BTN_X))
	{
		speed = 400.0f;
	}
	ctx.xmm[0].f32[0] = speed;
}

// GUNMech Walker Revamp
bool WalkerDismountAnywhereMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

void WalkerHoverMoveMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	if ((X86_LOAD_U8(ctx.esi.u32 + 0x104) & 0x8) == 0)
	{
		return;
	}

	// Get the vehicle stats
	const float friction = X86_LOAD_F32(ctx.esi.u32 + 0xE8);
	const float driveMult = X86_LOAD_F32(ctx.esi.u32 + 0xE0);
	const float walkFriction = X86_LOAD_F32(0x4721CC);

	if (friction <= 0.0f || driveMult <= 0.0f || walkFriction <= 0.0f)
	{
		return;
	}

	const float speed = driveMult / walkFriction;
	float stickX = X86_LOAD_F32(ctx.esi.u32 + 0xF8);
	float stickZ = X86_LOAD_F32(ctx.esi.u32 + 0x100);

	X86_STORE_F32(ctx.esi.u32 + 0x28, stickX * speed * friction);
	X86_STORE_F32(ctx.esi.u32 + 0x30, stickZ * speed * friction);
	X86_STORE_F32(ctx.esi.u32 + 0xE4, friction);
	X86_STORE_F32(ctx.esi.u32 + 0xE8, friction);

	// Rotation fix
	X86_STORE_F32(ctx.esi.u32 + 0xF8, 0.0f);
	X86_STORE_F32(ctx.esi.u32 + 0xFC, 0.0f);
	X86_STORE_F32(ctx.esi.u32 + 0x100, 0.0f);
}

// Restore EggBreaker0411 Digitize Effect
// true skips mov [edx+0x24A], al
bool EggBreakerDigitizeEffectMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// Disable Slide on B-press on ground and state extension
bool SlideStateExtensionMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t player = ctx.eax.u32;
	if (!Config::ReloadedMod || X86_LOAD_U32(player + 0x238) == 0)
	{
		return (X86_LOAD_U32(player + 0xA0) & 0x10) != 0;
	}

	const uint32_t behavior = GetBehaviorId(player);
	return behavior == BEHAVIOR_JUMP_DASH || behavior == BEHAVIOR_HOMING_ATTACK || behavior == BEHAVIOR_SPIN_DASH;
}

// Slide State Extension - Bad Transition Bugfix
void SlideBadTransitionMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod && (ctx.ecx.u32 == 0xF8 || ctx.ecx.u32 == 0xF9))
	{
		ctx.ecx.u32 = 0xFA;
	}
}

// Slide Steer 2 -> 4
void SlideSteerMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] *= Config::ReloadedMod ? 4.0f : X86_LOAD_F32(0x429F74);
}

static bool IsSlideHeld(uint32_t sliding)
{
	auto base = g_memory.base;
	const uint32_t player = X86_LOAD_U32(sliding + 0x4);
	const uint32_t input = X86_LOAD_U32(player + 0x218);

	// 0x18 = held actions @ 0x449CF8
	// & 0x60 = X or B
	return input != 0 && (X86_LOAD_U32(input + 0x18) & 0x60) != 0;
}

bool HoldableSlideMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod && IsSlideHeld(ctx.ebx.u32);
}

bool HoldableSlideReleaseMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod && !IsSlideHeld(ctx.ebx.u32);
}

// Fix Sliding on Steep Slopes
bool SlideSteepSlopeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// LightDash Transition Expansion
bool LightDashTransitionMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t player = ctx.eax.u32;
	if ((X86_LOAD_U32(player + 0xA0) & 0x8) != 0)
	{
		return true;
	}

	if (!Config::ReloadedMod)
	{
		return false;
	}

	const uint32_t behavior = GetBehaviorId(player);
	return behavior == BEHAVIOR_JUMP_DASH || behavior == BEHAVIOR_HOMING_JUMP || behavior == BEHAVIOR_LAUNCH;
}

// LightDash Exit State Change to Launched for Westopolis
void LightDashExitStateMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod || X86_LOAD_U32(g_currentStageId) != 100)
	{
		X86_STORE_U32(ctx.esp.u32 + 0x3C, BEHAVIOR_FALL);
		return;
	}

	// InitializeArgsLaunch
	X86_STORE_U32(ctx.esp.u32 + 0x38, 0x448FAC);
	X86_STORE_U32(ctx.esp.u32 + 0x3C, BEHAVIOR_LAUNCH);
	X86_STORE_U32(ctx.esp.u32 + 0x40, LAUNCH_FLAG_NO_BLUR); // skydive
}

// No Radial Blur after LightDash (for the Westopolis skydive hack)
bool LightDashSkydiveBlurDisableMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t args = X86_LOAD_U32(ctx.esp.u32 + 0x1C);
	return (X86_LOAD_U32(args + 0x8) & LAUNCH_FLAG_NO_BLUR) != 0;
}

// WallRun Speed 500 -> 550
void WallRunSpeedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod ? 550.0f : 500.0f);
}

// Triangle Jump Speed 450 -> 1000
void TriangleJumpSpeedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod ? 1000.0f : 450.0f);
}

// No Tri-Jump Exit
bool NoTriJumpExitMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return false;
	}

	ctx.xmm[0].f32[0] = 0.0f;
	return true;
}

// Walk Turn Speed 15 -> 20
void WalkTurnSpeedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod ? 20.0f : 15.0f);
}

// No Skidding
bool NoSkiddingMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod && !Config::ReloadedModOriginalSkid;
}

// Keep Melee Weapons when Mounting Vehicles
bool KeepMeleeOnVehicleMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// CarType Vehicles Revamp
static bool IsCarRevamp()
{
	return Config::ReloadedMod && !Config::ReloadedModOriginalCarPhysics;
}

bool CarGasReleaseMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!IsCarRevamp())
	{
		return false;
	}

	ctx.xmm[1].f32[0] = ctx.xmm[0].f32[0];
	return true;
}

bool CarBrakeButtonMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint8_t held = X86_LOAD_U8(ctx.eax.u32 + ctx.edx.u32 * 8);
	return (held & (IsCarRevamp() ? PJSPeripheral::BTN_Y : PJSPeripheral::BTN_B)) != 0;
}

bool CarRevampMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return IsCarRevamp();
}

bool CarDriftHoldableMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!IsCarRevamp())
	{
		return false;
	}

	const uint32_t car = ctx.edi.u32;
	const float dt = X86_LOAD_F32(ctx.esp.u32 + 0x14);
	const auto* pad = GetPadSlot(X86_LOAD_U32(car + 0x260));
	const bool held = pad && (pad->buttons & (PJSPeripheral::BTN_WHITE | PJSPeripheral::BTN_BLACK)) != 0;
	const float drift = X86_LOAD_F32(car + 0x70);

	// ease back
	const float pitchRate = X86_LOAD_F32(0x51CD50 + X86_LOAD_U32(car + 0x25C) * 0xAC);
	X86_STORE_F32(car + 0x7C, std::max(X86_LOAD_F32(car + 0x7C) - pitchRate * dt, 0.0f));

	// remove the broken 'drift exit' timers that they never finished
	X86_STORE_F32(car + 0x74, 0.0f);
	X86_STORE_F32(car + 0x78, 0.0f);
	if (held)
	{
		X86_STORE_F32(car + 0x70, std::max(drift, 0.0f) + dt);
	}
	else if (drift > 0.0f)
	{
		// mark drift end
		X86_STORE_F32(car + 0x70, 0.0f);
		X86_STORE_U8(car + 0x2B4, 1);
	}
	return true;
}

// Fix steering during drift
bool CarDriftRecoverySteerMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return IsCarRevamp();
}

static bool IsSuperAirSaucer()
{
	return Config::ReloadedMod && !Config::ReloadedModOriginalAirSaucerPhysics;
}

// Super AirSaucer JumpStrength 100 -> 200
void AirSaucerJumpMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, IsSuperAirSaucer() ? 200.0f : 100.0f);
}

// AirSaucer Replace Second Jump with Stomp (update second jump)
void AirSaucerStompMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	float strength = 100.0f;
	if (IsSuperAirSaucer())
	{
		strength = Config::ReloadedModOriginalAirSaucerDoubleJump ? 200.0f : -400.0f;
	}
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, strength);
}

// Heal Unit Explosion Hitbox Size 20 -> 45
void HealUnitBlastRadiusMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = Config::ReloadedMod ? 45.0f : X86_LOAD_F32(0x42B45C);
}

// Triple Spring Origin Shift 10 -> 5
void TripleSpringOriginMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod)
	{
		ctx.xmm[0].f32[0] = 5.0f;
	}
}

// Enemy AppearType WARP Faster Spawn 1 -> 0.25
void EnemyWarpAppearTimeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = Config::ReloadedMod ? 0.25f : X86_LOAD_F32(0x46B438);
}

// ElecCode Enemy Faster Movement Speed 50 -> 350
void ElecCodeSpeedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod && !Config::ReloadedModOriginalElecCodeEnemySpeed ? 350.0f : 50.0f);
}

// Katana LV1 Ammo 4 -> 18
void KatanaLv1AmmoMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	X86_STORE_U32(0x5594B0, Config::ReloadedMod ? 18 : 4);
}

// Katana LV2 Ammo 8 -> 32
void KatanaLv2AmmoMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	X86_STORE_U32(0x5594E0, Config::ReloadedMod ? 32 : 8);
}

// SatelliteLaser LV1 Ammo 6 -> 10
void SatelliteLaserLv1AmmoMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	X86_STORE_U32(0x559514, Config::ReloadedMod ? 10 : 6);
}

// SatelliteLaser LV2 Ammo 10 -> 24
void SatelliteLaserLv2AmmoMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	X86_STORE_U32(0x559544, Config::ReloadedMod ? 24 : 10);
}

// CannonAndroid Damage 8 -> 16 (direct hits)
X86_FUNC_IMPL(__imp__sub_2621A0);
X86_OVERRIDE_FUNC(sub_2621A0)
{
	if (!Config::ReloadedMod)
	{
		__imp__sub_2621A0(ctx, base);
		return;
	}

	// Bazooka (type 1 cannon bullets), loading incase other mods change it
	const float damage = X86_LOAD_F32(0x5247B4);
	X86_STORE_F32(0x5247B4, damage * 2.0f);
	__imp__sub_2621A0(ctx, base);
	X86_STORE_F32(0x5247B4, damage);
}

// Global Lock-On Revamp
void LockOnShotIntervalMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	X86_STORE_F32(ctx.esi.u32 + 0x54, std::max(X86_LOAD_F32(ctx.esi.u32 + 0x54) - 0.175f, 0.0f));
}

// SolidCollision Triggers are GroundCollision
void SolidCollisionTriggerMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.eax.u32 = Config::ReloadedMod ? 0x101 : 0x102;
}

// Cannon & Lock On Weapons No Knockback
bool WeaponNoKnockbackMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod && !Config::ReloadedModOriginalWeaponKnockback;
}

// EscapePlane / EggBalloon Stat Adjustment
bool ChaseObjNoInvulnerabilityMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

static bool IsEscapePlaneHealth()
{
	return Config::ReloadedMod && !Config::ReloadedModOriginalEscapePodHealth;
}

// EscapePlane phase 2 600 -> 1600
void EscapePlanePhase2MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = IsEscapePlaneHealth() ? 1600.0f : X86_LOAD_F32(0x456B0C);
}

// EscapePlane phase 3 400 -> 1000
void EscapePlanePhase3MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = IsEscapePlaneHealth() ? 1000.0f : X86_LOAD_F32(0x44BB18);
}

// EscapePlane phase 4 200 -> 300
void EscapePlanePhase4MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = IsEscapePlaneHealth() ? 300.0f : X86_LOAD_F32(0x449B38);
}

// EggBalloon phase 2 300 -> 800
void EggBalloonPhase2MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = Config::ReloadedMod ? 800.0f : X86_LOAD_F32(0x454388);
}

// EggBalloon phase 3 200 -> 500
void EggBalloonPhase3MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = Config::ReloadedMod ? 500.0f : X86_LOAD_F32(0x449B38);
}

// EggBalloon phase 4 100 -> 200
void EggBalloonPhase4MidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = Config::ReloadedMod ? 200.0f : X86_LOAD_F32(0x42BA7C);
}

// EggBalloon gauge 300 -> 800
void EggBalloonGaugeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, Config::ReloadedMod ? 800.0f : 300.0f);
}

// Beating Last Story unlocks Expert
void LastStoryUnlocksExpertMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod)
	{
		X86_STORE_U8(ctx.esi.u32 + 0x16C9, 1);
	}
}

bool ARankRecountMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// Devil Doom Boss Has No Awake Music
void DevilDoomAwakePlayMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod && X86_LOAD_U32(g_currentStageId) == 710)
	{
		ctx.ecx.u32 = 7; // invalid (purposely skip)
	}
}

void DevilDoomAwakeStopMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod && X86_LOAD_U32(g_currentStageId) == 710)
	{
		ctx.ecx.u32 = 0;
	}
}

// Devil Doom and The Last Way are Ranked Missions
bool DevilDoomAndLastWayRankedMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

static bool IsExpertRingLoss()
{
	auto base = g_memory.base;
	return Config::ReloadedMod && !Config::ReloadedModOriginalExpertRingDropBehavior && X86_LOAD_U8(0x54DE55) != 0;
}

// Expert Ring Loss
void ExpertRingLossMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (IsExpertRingLoss())
	{
		ctx.esi.u32 = 999;
	}
}

// Expert ring radius 5 -> 0
void ExpertRingRadiusMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_F32(ctx.esp.u32, IsExpertRingLoss() ? 0.0f : 5.0f);
}

// Expert ring lifetime 10 -> 0.8
void ExpertRingLifetimeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = IsExpertRingLoss() ? 0.8f : X86_LOAD_F32(0x468058);
}

// Expert ring blink 1.5 -> 10
void ExpertRingBlinkMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.xmm[0].f32[0] = IsExpertRingLoss() ? 10.0f : X86_LOAD_F32(0x468054);
}

// Disable Diablon Warp Player Function
bool DiablonNoWarpMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

static bool IsChaosPowerActive()
{
	if (!Config::ReloadedMod)
	{
		return false;
	}

	auto base = g_memory.base;
	const uint32_t gameState = X86_LOAD_U32(0x54963C);
	return gameState != 0 && (X86_LOAD_U32(gameState + 0x714) != 0 || X86_LOAD_U32(gameState + 0x720) != 0);
}

// SuperShadow Always Has Fully Charged Spears while ChaosPowers Active
void ChaosSpearChargeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (IsChaosPowerActive())
	{
		X86_STORE_F32(ctx.esp.u32 + 0x14, 1.0f);
	}
}

bool ChaosSpearMaxChargeMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return IsChaosPowerActive();
}

bool ChaosSpearChargedVoiceMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return IsChaosPowerActive();
}

// The Last Way Inclusive Scoring
void LastWayInclusiveScoringMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod && X86_LOAD_U32(g_currentStageId) == 700)
	{
		X86_STORE_U32(ctx.esi.u32, X86_LOAD_U32(ctx.esi.u32) + ctx.ecx.u32);
	}
}

static bool CountsAsARank(uint32_t rank)
{
	return static_cast<int32_t>(rank) <= (Config::ReloadedMod && !Config::ReloadedModDisableSRank ? 1 : 0);
}

// S Rank System
bool SRankSystemEcxMidAsmHook(X86Context& __restrict ctx, uint8_t* base) { return CountsAsARank(ctx.ecx.u32); }
bool SRankSystemEaxMidAsmHook(X86Context& __restrict ctx, uint8_t* base) { return CountsAsARank(ctx.eax.u32); }
bool SRankSystemMemMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return CountsAsARank(X86_LOAD_U32(ctx.ebp.u32));
}

// Select Mode cursor no reset
bool SelectModeCursorNoResetMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return false;
	}

	if (X86_LOAD_U32(0x5285E4) == 0 && X86_LOAD_U32(0x5285E8) == 0)
	{
		X86_STORE_U32(0x5285E8, 2);
	}
	return true;
}

// Life System Modifier
bool LifeCounterFloorMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return static_cast<int32_t>(ctx.eax.u32) <= (Config::ReloadedMod ? 1 : 0);
}

void LifeRestartMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	ctx.esp.u32 -= 4;
	X86_STORE_U32(ctx.esp.u32, Config::ReloadedMod ? 0 : UINT32_MAX);
}

bool EggDealerLifeEasterEggMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (ctx.eax.u32 != (Config::ReloadedMod ? 1u : 0u))
	{
		return false;
	}
	ctx.eax.u32 = 0; // enable easter egg
	return true;
}

// Respawn at last activated checkpoint
bool RespawnLastCheckpointMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// Time Bonus Penalty Always Active
bool TimeBonusPenaltyMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod;
}

// In Stage Missions Unlocked (always switchable)
void InStageMissionsAlwaysUnlockedSwitchableMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod)
	{
		X86_STORE_U8(ctx.esi.u32 + 0x14, 1);
	}
}

// Mission Focus
static bool s_missionFocus = false;

void MissionFocusDpadMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	if (ctx.eax.u32 & 0x1000) // d-pad up
	{
		s_missionFocus = false;
	}
	else if (ctx.eax.u32 & 0xE00) // any other d-pad
	{
		s_missionFocus = true;
	}
}

void MissionFocusPauseMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (Config::ReloadedMod)
	{
		s_missionFocus = true;
	}
}

void MissionFocusClearMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	s_missionFocus = false;
}

bool MissionFocusSkipMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return Config::ReloadedMod && s_missionFocus;
}

// Music Update Ending Tracks and Lava Shelter Boss
void MusicUpdateEndingTracksMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	static const uint32_t vox07 = AllocGuestString("sng_vox07.adx");
	static const uint32_t stg0613 = AllocGuestString("sng_stg0613.adx");

	const int32_t stage = ctx.eax.i32;

	// ending different songs
	if (stage < 614)
	{
		X86_STORE_U32(0x5297A4, stage < 611 ? vox07 : stage == 611 ? 0x447114 : 0x4470D4); // vox02, vox06
	}
	else
	{
		X86_STORE_U32(0x5297A8, stage == 617 ? 0x4470E4 : vox07); // vox05
	}

	const bool eggDealerLavaShelter = stage == 614;
	X86_STORE_U32(0x51DC9C, eggDealerLavaShelter ? stg0613 : 0x446F04);
	X86_STORE_U32(0x51DCA4, eggDealerLavaShelter ? stg0613 : 0x446F04);
	X86_STORE_U8(0x43485A, eggDealerLavaShelter ? '3' : '2'); // 0612
}

// Expert Ending SFD Overrides
void ExpertEndingMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	const bool expert = ctx.eax.u32 == 6;

	// sng_EV_music3 -> sng_EV_music4
	X86_STORE_U8(0x446DA0, expert ? '4' : '3');

	// .sfd -> .flo { name, fileEntry }
	static constexpr uint32_t movies[][2] =
	{
		{ 0x446C82, 0x4416F6 }, // E4101
		{ 0x446C6A, 0x4416DE }, // E4201
		{ 0x446C76, 0x4416EA }, // E4102
		{ 0x446C52, 0x4416C6 }, // E8003
		{ 0x446C46, 0x4416BA }, // E8201
		{ 0x479973, 0x435867 }, // OP
	};

	for (const auto& [name, entry] : movies)
	{
		memcpy(base + name, expert ? "flo" : "sfd", 3);
		memcpy(base + entry, expert ? "FLO" : "SFD", 3);
	}
}

// Ghost Red Rings (gdt -> gd2)
void GhostRedRingsGDTRedirectMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod || Config::ReloadedModDisableGhostRedRings)
	{
		return;
	}

	bool allCollected = true;
	for (uint32_t i = 0; i < 5; i++)
	{
		allCollected &= X86_LOAD_U32(0x54CD0C + i * 4) != UINT32_MAX;
	}

	// gd2 if all 5 keys already
	X86_STORE_U8(0x464F6A, allCollected ? '2' : 't');
	if (allCollected)
	{
		for (uint32_t i = 0; i < 5; i++)
		{
			X86_STORE_U32(0x54CD0C + i * 4, UINT32_MAX);
		}
	}
}

bool GhostRedRingsSaveSkipMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod || Config::ReloadedModDisableGhostRedRings)
	{
		return false;
	}

	const uint32_t save = ctx.ecx.u32 + X86_LOAD_U32(0x54DDDC) * 0x60 + 0xF4;
	for (uint32_t i = 0; i < 5; i++)
	{
		const uint32_t ring = X86_LOAD_U32(0x54CD0C + i * 4);
		if (ring != UINT32_MAX)
		{
			X86_STORE_U32(save + i * 4, ring);
		}
	}
	return true;
}

// Melee Weapon Core Revamp
static bool s_reloadedMelee[4] = {}; // 4P support
static std::unordered_map<uint32_t, float> s_meleeRehitTimes;

static uint32_t GetReloadedMeleePad(uint32_t player)
{
	auto base = g_memory.base;
	if (!Config::ReloadedMod || !player)
	{
		return 0;
	}

	const uint32_t input = X86_LOAD_U32(player + 0x218);
	const uint32_t pad = input ? X86_LOAD_U32(input + 0x10) : 0;
	const uint32_t slot = (pad - 0x541A68) / 0x2C;
	if (pad < 0x541A68 || pad != 0x541A68 + slot * 0x2C || slot >= 4 || !s_reloadedMelee[slot])
	{
		return 0;
	}
	return pad;
}

static bool HasMeleeWeapon(const X86Context& ctx, uint32_t player)
{
	auto base = g_memory.base;
	const uint32_t weapon = X86_LOAD_U32(player + 0x238);
	if (!weapon)
	{
		return false;
	}

	// Type 5 is melee in pWeapon
	const uint32_t getType = X86_LOAD_U32(X86_LOAD_U32(weapon) + 0x18);
	return GuestToHostFunction<uint32_t, CallConvention::Thiscall>(ctx, getType, weapon) == 5;
}

// RB to toggle reloaded mode (per pad)
void MeleeModeToggleMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t slot = (ctx.edx.u32 - 0x541A68) / 0x2C;
	if (Config::ReloadedMod && ctx.edx.u32 >= 0x541A68 && slot < 4 && (X86_LOAD_U32(ctx.edx.u32 + 0x8) & 0xFF) == PJSPeripheral::BTN_BLACK)
	{
		s_reloadedMelee[slot] = !s_reloadedMelee[slot];
	}
}

// hit count locked to 1 per coli hit
bool MeleeHitCountMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod)
	{
		return false;
	}

	X86_STORE_U32(ctx.esi.u32 + 0x40, 1);
	return true;
}

// reloaded melee rehit every 0.1s
static bool IsMeleeRehitWaiting(float time, float next)
{
	return time < next && next - time <= 0.1f;
}

// black knight mode (ignore per collision rules)
bool MeleeRehitMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	if (!Config::ReloadedMod || ctx.eax.u8_lo == 0)
	{
		return false;
	}

	const float time = X86_LOAD_F32(0x54DDC4);
	auto it = s_meleeRehitTimes.find(ctx.esi.u32);
	if (it != s_meleeRehitTimes.end() && IsMeleeRehitWaiting(time, it->second))
	{
		return false;
	}

	std::erase_if(s_meleeRehitTimes, [time](const auto& entry) { return !IsMeleeRehitWaiting(time, entry.second); });
	s_meleeRehitTimes[ctx.esi.u32] = time + 0.1f;
	return true;
}

// reloaded melee 'hold' mode
X86_FUNC_IMPL(__imp__sub_897A0);
X86_OVERRIDE_FUNC(sub_897A0)
{
	const uint32_t player = X86_LOAD_U32(X86_LOAD_U32(ctx.ecx.u32 + 0x28));
	__imp__sub_897A0(ctx, base);
	if (ctx.eax.u8_lo == 0 && GetReloadedMeleePad(player) && HasMeleeWeapon(ctx, player))
	{
		ctx.eax.u8_lo = 1;
	}
}

// reloaded melee ignore motion collision flag
bool MeleeMotionColliMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	return ctx.eax.u32 == 5 && GetReloadedMeleePad(X86_LOAD_U32(ctx.edi.u32 + 0x58));
}

// reloaded melee X + LT rapid attack
void MeleeRapidMotionMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t player = X86_LOAD_U32(ctx.esi.u32 + 0x58);
	const uint32_t pad = GetReloadedMeleePad(player);
	if (!pad || !HasMeleeWeapon(ctx, player) || (X86_LOAD_U32(pad) & 0xFFFF) != (PJSPeripheral::BTN_X | PJSPeripheral::BTN_LEFT_TRIG))
	{
		return;
	}

	if (ctx.edi.u32 == 4)
	{
		const float speed = X86_LOAD_F32(ctx.esi.u32 + 0x43C) * 2.0f;
		X86_STORE_F32(ctx.esi.u32 + 0x43C, speed * speed);
	}
	X86_STORE_U32(ctx.esi.u32 + 0x420, 0x89);
	X86_STORE_U32(ctx.esi.u32 + 0x424, UINT32_MAX);
}

static uint32_t FindMtpEntry(uint32_t mtp, const char* name)
{
	auto base = g_memory.base;
	const uint32_t count = X86_LOAD_U16(mtp + 0x2);
	const uint32_t tableField = X86_LOAD_U32(mtp + 0x10);
	const uint32_t table = tableField < mtp ? mtp + tableField : tableField;
	const uint32_t nameBase = X86_LOAD_U32(table) < mtp ? mtp : 0;
	for (uint32_t i = 0; i < count; i++)
	{
		const uint32_t entry = table + i * 0xC;
		if (strcmp((const char*)(base + nameBase + X86_LOAD_U32(entry)), name) == 0)
		{
			return entry;
		}
	}
	return 0;
}

// Embedded MTP edits (I just swap props and mtns) to solve modders being annoyed at the desync
// YOU'RE WELCOME DUEZA >:D
X86_FUNC_IMPL(__imp__sub_377720);
X86_OVERRIDE_FUNC(sub_377720)
{
	const uint32_t mtp = X86_LOAD_U32(ctx.esp.u32 + 0x4);
	__imp__sub_377720(ctx, base);
	if (!Config::ReloadedMod || !mtp)
	{
		return;
	}

	const uint32_t runAttack = FindMtpEntry(mtp, "sh_run_atc_eda");
	const uint32_t attack = FindMtpEntry(mtp, "sh_atc_eda");
	const uint32_t loop = FindMtpEntry(mtp, "sh_flot_side");

	if (runAttack && attack && loop)
	{
		X86_STORE_U32(runAttack + 0x4, X86_LOAD_U32(attack + 0x4));
		X86_STORE_U32(runAttack + 0x8, X86_LOAD_U32(loop + 0x8));
	}
}
// End Melee Weapon Core Revamp

void ReloadedValuePatches()
{
	if (!Config::ReloadedMod)
	{
		return;
	}

	auto base = g_memory.base;

	// 2P Unique Android JumpBall Colors
	for (uint32_t i = 0; i < 5; i++)
	{
		const uint32_t androidBalls[] = {
			2, // Tails (Yellow Android)
			4, // Amy (Pink Gun Android)
			1, // Sonic (Blue Gun Android)
			6, // Omega (Orange Android)
			7  // Vector (Green Cannon)
		};
		memcpy(base + 0x449D78 + (0xF + i) * 0x2C, base + 0x449D78 + androidBalls[i] * 0x2C, 0x14);
	}

	// Jump Ball Turn Speed 6.5 -> 25
	X86_STORE_F32(0x448414, 25.0f);

	// Chaos Blast Damage 12 -> 36
	X86_STORE_F32(0x4EBE4C, 36.0f);

	// Jump Attack Friction 10 -> 4
	X86_STORE_F32(0x4ED69C, 4.0f);

	// Light Dash ValidRing Angle 45 -> 75
	X86_STORE_F32(0x448708, 75.0f);

	// Homing Attack Turn Strength 8 -> 16
	X86_STORE_F32(0x448268, 16.0f);

	// Triangle Jump Recover Time 0.125 -> 0.000001
	X86_STORE_F32(0x448D1C, 0.000001f);

	// Walk Turn Speed 15 -> 20
	X86_STORE_F32(0x448100, 20.0f);

	// Instant HealUnit / BombUnit Server Respawn Time 4 -> 0
	X86_STORE_F32(0x46D784, 0.0f);

	// Pulley Speed 0.18 -> 0.25
	X86_STORE_F32(0x468A18, 0.25f);

	// == Idle Chatter Tweak ==
	// Repeat 20 -> 15
	X86_STORE_F32(0x449C80, 15.0f);
	// Idle 10 -> 7.5
	X86_STORE_F32(0x449C7C, 7.5f);
	// == Idle Chatter Tweak ==

	// == Super AirSaucer ==
	if (!Config::ReloadedModOriginalAirSaucerPhysics)
	{
		// RotSpeed 250 -> 8000
		X86_STORE_F32(0x471B10, 8000.0f);
		// GroundSpeed 270 -> 540
		X86_STORE_F32(0x471AF4, 540.0f);
		// AirSpeed 220 -> 440
		X86_STORE_F32(0x471AE4, 440.0f);
	}
	// == Super AirSaucer ==

	// EscapePlane Stat Adjustment 800 -> 2200
	if (!Config::ReloadedModOriginalEscapePodHealth)
	{
		X86_STORE_F32(0x46CED4, 2200.0f);
	}

	// EggBalloon Stat Adjustment 400 -> 1100
	X86_STORE_F32(0x46A2C0, 1100.0f);

	// == The Last Way idle lines ==
	// 0x1B7 -> 0x1E6
	X86_STORE_U32(0x44C55C, 0x1E6);
	// 0x1B8 -> 0x1E6
	X86_STORE_U32(0x44C560, 0x1E6);
	// 0x1E6 -> 0x1E7
	X86_STORE_U32(0x44C564, 0x1E7);
	// 0x1E6 -> 0x1E8
	X86_STORE_U32(0x44C568, 0x1E8);
	// == The Last Way idle lines ==

	// == Melee Ammo Adjustments ==
	// BlackSword 6 -> 18
	X86_STORE_U32(0x522BD4, 18);
	// DarkHammer 6 -> 18
	X86_STORE_U32(0x522D34, 18);

	// Torch0300 4 -> 12
	X86_STORE_U32(0x52421C, 12);

	// Stick0100 4 -> 12
	X86_STORE_U32(0x523FB4, 12);
	// Stick0200 4 -> 12
	X86_STORE_U32(0x523A2C, 12);
	// Stick0201 4 -> 12
	X86_STORE_U32(0x523A80, 12);
	// Stick0202 4 -> 12
	X86_STORE_U32(0x523984, 12);
	// Stick0301 4 -> 12
	X86_STORE_U32(0x523900, 12);
	// Stick0302 4 -> 12
	X86_STORE_U32(0x523888, 12);
	// Stick0400 4 -> 12
	X86_STORE_U32(0x523DB0, 12);
	// Stick0401 4 -> 12
	X86_STORE_U32(0x523EF4, 12);
	// Stick0402 4 -> 12
	X86_STORE_U32(0x523C3C, 12);
	// Stick0403 4 -> 12
	X86_STORE_U32(0x523CF0, 12);
	// Stick0404 4 -> 12
	X86_STORE_U32(0x523E7C, 12);
	// Stick0501 4 -> 12
	X86_STORE_U32(0x523BC4, 12);
	// Stick0502 4 -> 12
	X86_STORE_U32(0x523D20, 12);
	// Stick0503 4 -> 12
	X86_STORE_U32(0x523AD4, 12);
	// Stick0504 4 -> 12
	X86_STORE_U32(0x523D50, 12);
	// Stick0600 4 -> 12
	X86_STORE_U32(0x523C90, 12);
	// Stick0601 4 -> 12
	X86_STORE_U32(0x523E4C, 12);
	// Stick0602 4 -> 12
	X86_STORE_U32(0x523858, 12);
	// Stick0603 4 -> 12
	X86_STORE_U32(0x523CC0, 12);
	// Stick0604 4 -> 12
	X86_STORE_U32(0x5239D8, 12);
	// Stick0700 (The Last Way) 4 -> 12
	X86_STORE_U32(0x523930, 12);

	// EggSpear 6 -> 18
	X86_STORE_U32(0x523D80, 18);
	// SurvivalKnife 6 -> 18
	X86_STORE_U32(0x523B70, 18);
	// == Melee Ammo Adjustments ==

	// Light Dash Valid Ring Distance
	// 80 -> 100
	X86_STORE_F32(0x4ECDFC, 100.0f);

	// Slide Run Threshhold
	// 200 -> 75
	X86_STORE_F32(0x4EC7FC, 75.0f);

	// Spindash Startup Delay 0.25 -> 1
	X86_STORE_F32(0x4ED19C, 1.0f);

	// Strafe Walk Speed 30 -> 55
	X86_STORE_F32(0x4EC800, 55.0f);

	// Red Slime Speed 340 -> 425
	X86_STORE_F32(0x4EC3A8, 425.0f);

	// LaserRifle Damage Adjustment 3 -> 6
	X86_STORE_F32(0x525190, 6.0f);

	// TankCannon Ammo 4 -> 8
	X86_STORE_U32(0x524198, 8);
	// TankCannon Damage 16 -> 36
	X86_STORE_F32(0x5247C0, 36.0f);

	// Global Lock-On 0.1 -> 0.125
	X86_STORE_F32(0x474C00, 0.125f);
}
