#include <kernel/function.h>
#include <user/config.h>

// QOL limitation breaking patches for mods etc

// For testing 2P mod or performance testing; Force TBlock to always load every chunk
// bool loadAllChunks(float* a1, float* a2, float* a3)
// {
// 	return 1;
// }
// Uncomment below if wanting to test with all chunks loaded
//GUEST_FUNCTION_HOOK_STDCALL(sub_12700, loadAllChunks);

// Level Geo Unrestrict (For Custom Stages)
// Note: This will break some stages, e.g. Egg Dealer (Black Comet) or... basically most final bosses? and if you die in a certain area in GUN Fortress.
// This is because Sonic Team left visibility chunks pointing to old files. These are usually ignored due to this file restriction function.
// Enable with UnlockGeoColiNameRestrictions
X86_FUNC_IMPL(__imp__sub_56E00);
X86_OVERRIDE_FUNC(sub_56E00)
{
	uint32_t strAddr = X86_LOAD_U32(ctx.esp.u32 + 4);
	if (Config::UnlockGeoColiNameRestrictions && strAddr)
	{
		auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
		for (const char* p = (const char*)(base + strAddr); *p; p++)
		{
			if ((p[0] == 's' || p[0] == 'S') &&
				(p[1] == 't' || p[1] == 'T') &&
				(p[2] == 'g' || p[2] == 'G') &&
				isDigit(p[3]) && isDigit(p[4]) &&
				isDigit(p[5]) && isDigit(p[6]) &&
				p[7] == '_' &&
				isDigit(p[8]) && isDigit(p[9]))
			{
				ctx.eax.u32 = 1;
				ctx.esp.u32 += 4;
				return;
			}
		}
	}
	__imp__sub_56E00(ctx, base);
}

// Collision BVH Stack Overflow Fix
// TODO: This corrupts memory / needs to be reworked; Was good enough for a POC
// Original sub_302E30 inner BVH stack lives on the host stack (~32 entries).
// Large collision meshes create BVH trees deeper than 32 levels, silently
// overflowing into other stack variables and crashing.
//
// Redirects the BVH stack to a static guest memory buffer using mid-asm hooks.
// [[midasm_hook]] entries at 0x303375 and 0x30392B

// static constexpr uint32_t COLL_BVH_BUF_ADDR = 0x80000000;
//
// void BVHStackRedirectInit(X86Register& ecx, X86Register& esi)
// {
// 	auto base = g_memory.base;
//
// 	float tNear = X86_LOAD_F32(ecx.u32 + 4);
// 	float tFar = X86_LOAD_F32(ecx.u32 + 8);
//
// 	X86_STORE_U32(COLL_BVH_BUF_ADDR + 0, 0x0000FF00);
// 	X86_STORE_U32(COLL_BVH_BUF_ADDR + 4, COLL_BVH_BUF_ADDR);
// 	X86_STORE_F32(COLL_BVH_BUF_ADDR + 8, tNear);
// 	X86_STORE_F32(COLL_BVH_BUF_ADDR + 12, tFar);
//
// 	esi.u32 = COLL_BVH_BUF_ADDR + 16;
// }
//
// void BVHStackRedirectCmp(X86Register& eax)
// {
// 	eax.u32 = COLL_BVH_BUF_ADDR + 4;
// }
