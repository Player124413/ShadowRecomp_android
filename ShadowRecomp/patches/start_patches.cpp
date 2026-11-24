#include <kernel/function.h>
#include <user/config.h>
#include <os/logger.h>
#include "code_patches.h"

extern bool QuickBootConfig;
extern int Ts_StageID;
extern std::vector<int> Ts_EventIDs;
extern bool Ts_EventAll;
extern bool Ts_expertMode;

static bool IsEventBoot()
{
	return Ts_EventAll || !Ts_EventIDs.empty();
}

//stub unnecessary startup functions to force the game to boot

NTSTATUS TryOpenVolume(uint32_t* FileHandle)
{
	return X_STATUS_SUCCESS;
}

NTSTATUS MapLetterToDirectory(uint32_t* FileHandle, WORD* a2)
{
	return X_STATUS_SUCCESS;
}

int XMountUtilityDrive(int32_t a1)
{
	return TRUE;
}

int XAPILIBXGetParentalControlSetting(int a1)
{
	return X_STATUS_SUCCESS;
}

int sub_2B0164_r(int a1)
{
	return TRUE;
}

//infinite lives Cheat
X86_FUNC_IMPL(__imp__sub_22D120);
X86_OVERRIDE_FUNC(sub_22D120)
{
	if (Config::InfiniteLives)
	{
		if (Config::InfiniteLives && (int32_t)X86_LOAD_U32(ctx.esp.u32 + 4) < 0)
			X86_STORE_U32(ctx.esp.u32 + 4, 0);
	}

	__imp__sub_22D120(ctx, base);
}

static constexpr uint32_t g_MenuModeRequest = 0x54BA10;
static constexpr uint32_t g_MovieManager = 0x541808;
static constexpr uint32_t g_StageSequenceManager = 0x54DDFC;
static constexpr uint32_t g_StageSequenceMode = 0x5022A4;
static constexpr uint32_t g_StageSequenceStageID = 0x54C910;

static constexpr uint32_t GameMode_Menu = 4;
static constexpr uint32_t GameMode_StageSequence = 5;

static constexpr uint32_t MenuMode_TitleScreen = 1;
static constexpr uint32_t MenuMode_Library = 4;
static constexpr uint32_t MenuMode_OpeningMovie = 8;

// Quick Boot
// Force mode 3 on the boot intro to reach directly the title screen
static bool s_quickBootDone = false;
X86_FUNC_IMPL(__imp__sub_29EA60);
X86_OVERRIDE_FUNC(sub_29EA60)
{
	const bool quickBoot = Config::QuickBoot || QuickBootConfig;
	auto ptr = X86_LOAD_U32(ctx.ecx.u32 + 4);

	if (!s_quickBootDone && quickBoot)
	{
		if (X86_LOAD_U32(ptr + 0x64) < 3)
			X86_STORE_U32(ptr + 0x64, 3);

		s_quickBootDone = true;
	}

	__imp__sub_29EA60(ctx, base);

	// mode 3 asks again every frame until the menu manager swaps the task, so check every call
	if (quickBoot && X86_LOAD_U32(ptr + 0x64) == 3)
	{
		auto menuMode = X86_LOAD_U32(g_MenuModeRequest);

		if (menuMode && X86_LOAD_U32(menuMode) == MenuMode_OpeningMovie)
			X86_STORE_U32(menuMode, MenuMode_TitleScreen);
	}

	// Set task kill to solve OG bug where the FMV flag persists
	auto menuMode = X86_LOAD_U32(g_MenuModeRequest);
	auto movie = X86_LOAD_U32(g_MovieManager);
	if (movie && menuMode && X86_LOAD_U32(menuMode) == MenuMode_TitleScreen)
	{
		X86_STORE_U8(movie + 0xC, X86_LOAD_U8(movie + 0xC) | 0x1);
	}
}

// Always allow skipping the title screen logos
void AlwaysAllowLogoSkipMidAsmHook(X86Context& __restrict ctx, uint8_t* base) {}

/* -- BOOT TO EVENT START -- */

// The following code allows users to boot the game on an event directly through command arguments.
// e.g (--e id or --e all), the latter allows to play all events in a row.

// Memo: host stage 0 is a movie (sfd), any other value is the stage the event
// plays in. The game originally fills the queue by following one of the 326
// library routes. (sub_23BF90)
static constexpr uint32_t g_EventQueue = 0x54C934;
static constexpr uint32_t g_EventQueueCount = 0x54CB34;
static constexpr int EventQueueMaxSize = 64;

static constexpr uint32_t g_StoryTable = 0x519C08; // 65 rows of 17 dwords, one row per stage to stage transition.
static constexpr int StoryRowCount = 65;
static constexpr int StoryRowSize = 17 * 4;
static constexpr int StoryRowEventsOffset = 3 * 4;
static constexpr int StoryRowEventCount = 14;

struct StoryEvent
{
	int id;
	int host;
};

static std::vector<StoryEvent> s_eventPlaylist;
static bool s_eventQueuePending = false;
static size_t s_eventPlaylistPos = 0;

// the game goes back to an empty library after playback, we will send it to the title screen instead
static bool s_eventReturnToTitle = false;

// every event of every route sorted by id. The ids follow the story
// order (e.g 1xxx first stage up to 7xxx endings and 8xxx last story).
static std::vector<StoryEvent> GetStoryEvents(uint8_t* base)
{
	std::vector<StoryEvent> events;

	for (int row = 0; row < StoryRowCount; row++)
	{
		const uint32_t rowEvents = g_StoryTable + row * StoryRowSize + StoryRowEventsOffset;

		for (int i = 0; i < StoryRowEventCount; i++)
		{
			const uint32_t entry = X86_LOAD_U32(rowEvents + i * 4);

			if (!entry)
				continue;

			const int id = X86_LOAD_U32(entry);
			const int host = X86_LOAD_U32(entry + 4);

			// the same event shows up in several rows, keep one
			auto it = std::find_if(events.begin(), events.end(), [id](const StoryEvent& e) { return e.id == id; });

			if (it == events.end())
				events.push_back({ id, host });
		}
	}

	std::sort(events.begin(), events.end(), [](const StoryEvent& a, const StoryEvent& b) { return a.id < b.id; });

	return events;
}

static void BuildEventPlaylist(uint8_t* base)
{
	const std::vector<StoryEvent> storyEvents = GetStoryEvents(base);

	s_eventPlaylist.clear();
	s_eventPlaylistPos = 0;

	if (Ts_EventAll)
	{
		s_eventPlaylist = storyEvents;
		return;
	}

	for (int eventId : Ts_EventIDs)
	{
		auto it = std::find_if(storyEvents.begin(), storyEvents.end(), [eventId](const StoryEvent& e) { return e.id == eventId; });

		if (it == storyEvents.end())
		{
			LOGFN_WARNING("Event {} is not a playable story event, skipping it.", eventId);
			continue;
		}

		s_eventPlaylist.push_back(*it);
	}
}

// the queue can hold 64 events
static void FillEventQueue(uint8_t* base)
{
	const size_t left = s_eventPlaylist.size() - s_eventPlaylistPos;
	const int count = int(std::min<size_t>(left, EventQueueMaxSize));

	for (int i = 0; i < count; i++)
	{
		const StoryEvent& event = s_eventPlaylist[s_eventPlaylistPos + i];

		X86_STORE_U32(g_EventQueue + i * 8, event.id);
		X86_STORE_U32(g_EventQueue + i * 8 + 4, event.host);
	}

	X86_STORE_U32(g_EventQueueCount, count);
	s_eventPlaylistPos += count;
}

// Memo: both boot to level and event start the same stage sequence
static void PrepareStageSequenceBoot(const X86Context& ctx, uint8_t* base)
{
	// reset the stage sequence state
	GuestToHostFunction<void, CallConvention::Thiscall>(ctx, sub_258610, g_StageSequenceManager);

	if (Ts_StageID != -1 && Ts_expertMode)
	{
		ToggleExpertMode();
		Ts_expertMode = false;
		auto saveData = X86_LOAD_U32(0x549640);
		X86_STORE_U32(saveData + 0xA0, 0x5); // Set lives to 0x5 otherwise expert mode will default to 0 (todo: add check if we add force load save here).
	}

	// we need to assign the pad num to the controller data
	// otherwise Vibration::Manager will crash the game with a vector out of range
	uint32_t padState = X86_LOAD_U32(0x5356F0);

	if (!padState)
		padState = GuestToHostFunction<uint32_t, CallConvention::Cdecl>(ctx, sub_11410);

	if (padState) // set pad num per player
	{
		X86_STORE_U32(padState + 404, 0);
		X86_STORE_U32(padState + 408, 1);
	}
}

// SwapGameMode
// boot directly into a stage when --l <stage id> or --e <event id> is used
// we first force game mode to 5 (StageSequence::Manager)
// then assign the new event or stage id needed.
static bool s_bootRedirectDone = false;
X86_FUNC_IMPL(__imp__sub_5C5E0);
X86_OVERRIDE_FUNC(sub_5C5E0)
{
	const uint32_t gameMode = ctx.esp.u32 + 4;
	const bool stageBoot = Ts_StageID != -1;
	const bool eventBoot = IsEventBoot();

	if (!s_bootRedirectDone && (stageBoot || eventBoot) && X86_LOAD_U32(gameMode) == GameMode_Menu)
	{
		s_bootRedirectDone = true;
		X86_STORE_U32(gameMode, GameMode_StageSequence);

		if (stageBoot)
		{
			X86_STORE_U32(g_StageSequenceMode, 0x0);
			X86_STORE_U32(g_StageSequenceStageID, Ts_StageID);
		}
		else
		{
			X86_STORE_U32(g_StageSequenceMode, 0x7); // sequence library playback
			BuildEventPlaylist(base);
			s_eventQueuePending = true;
			s_eventReturnToTitle = true;

			LOGN("--event: starting playback");
		}

		PrepareStageSequenceBoot(ctx, base);
	}

	__imp__sub_5C5E0(ctx, base);
}

// Build the event queue.
// We fill it with our own playlist instead and then clear the
// sequence mode so the stage sequence does not start the library again after using it.
X86_FUNC_IMPL(__imp__sub_23BF90);
X86_OVERRIDE_FUNC(sub_23BF90)
{
	if (!s_eventQueuePending)
	{
		__imp__sub_23BF90(ctx, base);
		return;
	}

	s_eventQueuePending = false;
	X86_STORE_U32(g_StageSequenceMode, -1);

	FillEventQueue(base);

	// we skip the original code, so adjust esp.
	ctx.eax.u32 = 1;
	ctx.esp.u32 += 4;
}

// event queue pop
// Since playback goes back to the main menu once the queue is empty, we can add the 
// next batch right when the final element is removed.
X86_FUNC_IMPL(__imp__sub_23C410);
X86_OVERRIDE_FUNC(sub_23C410)
{
	__imp__sub_23C410(ctx, base);

	if (s_eventPlaylistPos < s_eventPlaylist.size() && X86_LOAD_U32(g_EventQueueCount) == 0)
		FillEventQueue(base);
}

// menu first screen
// go back to title screen once events are done playing.
X86_FUNC_IMPL(__imp__sub_28C180);
X86_OVERRIDE_FUNC(sub_28C180)
{
	__imp__sub_28C180(ctx, base);

	if (!s_eventReturnToTitle)
		return;

	s_eventReturnToTitle = false;
	s_eventPlaylist.clear();
	s_eventPlaylistPos = 0;

	auto menuMode = X86_LOAD_U32(g_MenuModeRequest);

	if (menuMode && X86_LOAD_U32(menuMode) == MenuMode_Library)
		X86_STORE_U32(menuMode, MenuMode_TitleScreen);
}

/* -- BOOT TO EVENT END -- */

GUEST_FUNCTION_HOOK_STDCALL(sub_2B0CE1, TryOpenVolume);
GUEST_FUNCTION_HOOK_STDCALL(sub_2B0C27, MapLetterToDirectory); //this stub will trigger the lack of space after pressing start
GUEST_FUNCTION_HOOK_STDCALL(sub_2AF4B3, XMountUtilityDrive);

GUEST_FUNCTION_HOOK(sub_2B0164, sub_2B0164_r);
