#include <kernel/function.h>
#include <app.h>
#include <sdl_events.h>
#include <input/input.h>
#include "global.h"
#include "multiplayer_mod_patches.h"
#include <os/logger.h>

uint32_t g_playerStructAddrs[4] = {};
bool isAwakeAboutToInvoke = false;
bool isAwakeAboutToRevoke = false;

RwV3d* GetPlayerPos(uint32_t slot)
{
	return g_playerStructAddrs[slot] != 0 ? (RwV3d*)g_memory.Translate(g_playerStructAddrs[slot] + 4) : nullptr;
}

//Awake Set Chaos Control / Blast mode
X86_FUNC_IMPL(__imp__sub_8E690);
X86_OVERRIDE_FUNC(sub_8E690)
{
	isAwakeAboutToInvoke = true;
	auto mode = X86_LOAD_U32(ctx.esp.u32 + 16);
	__imp__sub_8E690(ctx, base);
	input::SetChaosAura(mode == 0 ? ELedAura::LED_AURA_VILLAIN : ELedAura::LED_AURA_HERO);
}

//Awake destructor
X86_FUNC_IMPL(__imp__sub_8E9A0);
X86_OVERRIDE_FUNC(sub_8E9A0)
{
	const uint32_t glowClone = PopAwakeGlowClone(ctx.ecx.u32);
	isAwakeAboutToRevoke = true;
	__imp__sub_8E9A0(ctx, base);
	if (glowClone != 0)
	{
		GuestToHostFunction<uint32_t, CallConvention::Cdecl>(ctx, sub_2CB040, glowClone);
	}
	App::s_chaosMode = ELedAura::LED_AURA_NEUTRAL;
}

// Player ctor
X86_FUNC_IMPL(__imp__sub_96AD0);
X86_OVERRIDE_FUNC(sub_96AD0)
{
	auto playerAddr = ctx.ecx.u32;
	__imp__sub_96AD0(ctx, base);
	int slot = -1;
	int freeSlot = -1;
	for (uint32_t i = 0; i < 4; i++)
	{
		// This should never happen (ctor on level init)
		// But just incase (maybe something weird with extended multiplayer / events later)
		if (g_playerStructAddrs[i] == playerAddr)
		{
			slot = i;
			break;
		}

		if (g_playerStructAddrs[i] == 0 && freeSlot == -1)
		{
			freeSlot = i;
		}
	}

	if (slot == -1)
	{
		slot = freeSlot;
	}

	if (slot == -1) // exit early (only track 4 slots)
	{
		return;
	}

	g_playerStructAddrs[slot] = playerAddr;
	LOGFN("Player Slot {} Struct {:x} Ptr {}", slot, playerAddr, (void*)GetPlayerPos(slot));
}

// Player dtor @ sub_97320 (not this func)
// Player ptr ctor created passed to sub_96B50 (this func), use to mark destroyed players
X86_FUNC_IMPL(__imp__sub_96B50);
X86_OVERRIDE_FUNC(sub_96B50)
{
	auto playerAddr = ctx.ecx.u32;
	__imp__sub_96B50(ctx, base);

	for (uint32_t i = 0; i < 4; i++)
	{
		if (g_playerStructAddrs[i] == playerAddr)
		{
			g_playerStructAddrs[i] = 0;
			LOGFN("Player Slot {} Destroyed {:x}", i, playerAddr);
			break;
		}
	}
}

// Originally clones SHADOW_FOOTEFF_R.DFF 3 times, make it clone a 4th and assign to slot 0x68 (bone 10).
// Constructor: 0x8DB90 [aka anonymous_namespace::Jet::Jet]
// NOTE: Destructor/Hide/Show don't need patches because the game expects all 4 slots to be filled but 4th is null in OG
// Destructor: 0x8D6B0
// Show Flame: 0x8CC50
// Hide Flame: 0x8CC80
// Slots: 0x5C (bone 7), 0x60 (bone 8), 0x64 (bone 9), 0x68 (bone 10)
void JetEffectRestoreFourthFlameMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t jet = ctx.esi.u32;
	const uint32_t firstFlame = X86_LOAD_U32(jet + 0x5C);
	if (firstFlame != 0 && X86_LOAD_U32(jet + 0x68) == 0)
	{
		// 0x2CB460 - model clone func
		const uint32_t clone = GuestToHostFunction<uint32_t, CallConvention::Cdecl>(ctx, sub_2CB460, firstFlame);
		if (clone != 0)
		{
			X86_STORE_U32(jet + 0x68, clone);
		}
	}
}

// LightDash stuck spam fix
bool LightDashStuckSpamFixMidAsmHook(X86Context& __restrict ctx, uint8_t* base)
{
	const uint32_t lightDash = ctx.ebx.u32;
	const uint32_t player = X86_LOAD_U32(lightDash + 0x4);
	float ahead = 0.0f;
	float prevAhead = 0.0f;
	for (int i = 0; i < 3; i++)
	{
		const float target = X86_LOAD_F32(lightDash + 0x30 + i * 4);
		const float dir = X86_LOAD_F32(lightDash + 0x24 + i * 4);
		ahead += (target - X86_LOAD_F32(player + 0x4 + i * 4)) * dir;
		prevAhead += (target - X86_LOAD_F32(player + 0xD0 + i * 4)) * dir;
	}
	return ahead >= 0.0f && prevAhead >= 0.0f;
}
