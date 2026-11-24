#include "boot_precompile.h"

#include <gpu/video.h>
#include <gpu/frame_pacing/frame_pacer.h>
#include <gpu/imgui/imgui_snapshot.h>
#include <locale/locale.h>
#include <ui/game_window.h>
#include <ui/imgui_utils.h>
#include <atomic>
#include <chrono>
#include <future>

static constexpr auto SHOW_DELAY = std::chrono::milliseconds(1000);

static constexpr float BASE_WIDTH = 640.0f;
static constexpr float BASE_HEIGHT = 480.0f;

static ImFont* g_fntSeurat;
static std::atomic<float> g_progress{ 0.0f };

void BootPrecompile::Init()
{
    g_fntSeurat = ImFontAtlasSnapshot::GetFont("FOT-SeuratPro-DB.otf");
}

void BootPrecompile::Draw()
{
    if (!s_isVisible)
    {
        return;
    }

    auto drawList = ImGui::GetBackgroundDrawList();
    auto& displaySize = ImGui::GetIO().DisplaySize;

    drawList->AddRectFilled({ 0, 0 }, displaySize, IM_COL32(0, 0, 0, 255));

    float scale = std::min(displaySize.x / BASE_WIDTH, displaySize.y / BASE_HEIGHT);
    float centreX = displaySize.x * 0.5f;
    float centreY = displaySize.y * 0.5f;

    float barWidth = 400.0f * scale;
    float barHeight = 16.0f * scale;
    ImVec2 barMin(centreX - barWidth * 0.5f, centreY - barHeight * 0.5f);
    ImVec2 barMax(centreX + barWidth * 0.5f, centreY + barHeight * 0.5f);

    float fontSize = 18.0f * scale;
    float smallFontSize = 16.0f * scale;

    const char* headerText = Localise("Installer_Page_Compiling").c_str();
    auto headerSize = g_fntSeurat->CalcTextSizeA(fontSize, FLT_MAX, 0, headerText);
    ImVec2 headerPos(centreX - headerSize.x * 0.5f, barMin.y - headerSize.y - 12.0f * scale);
    DrawTextWithShadow(g_fntSeurat, fontSize, headerPos, IM_COL32(255, 255, 255, 255), headerText, 1.5f, 1.0f, IM_COL32(0, 0, 0, 127));

    float progress = g_progress;

    drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 40, 255));
    drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * progress, barMax.y), IM_COL32(255, 255, 0, 255));

    char percentText[32];
    snprintf(percentText, sizeof(percentText), "%.1f%%", progress * 100.0f);
    auto percentSize = g_fntSeurat->CalcTextSizeA(smallFontSize, FLT_MAX, 0, percentText);
    ImVec2 percentPos(centreX - percentSize.x * 0.5f, barMax.y + 8.0f * scale);
    DrawTextWithShadow(g_fntSeurat, smallFontSize, percentPos, IM_COL32(255, 255, 255, 255), percentText, 1.5f, 1.0f, IM_COL32(0, 0, 0, 127));
}

void BootPrecompile::Run()
{
    auto task = std::async(std::launch::async, []
        {
            Video::PrecompileShadersAndPipelines([](float progress) { g_progress = progress; });
        });

    if (task.wait_for(SHOW_DELAY) != std::future_status::ready)
    {
        s_isVisible = true;
        FramePacer framePacer;
        framePacer.SetFpsLimit(60.0);
        while (task.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            SDL_PumpEvents();
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    std::_Exit(0);
                }
            }
            GameWindow::Update();
            Video::Present(nullptr);
            framePacer.EndFrame(FramePacer::WaitMode::SleepOnly);
        }
        g_progress = 1.0f;
        GameWindow::Update();
        Video::Present(nullptr);
        s_isVisible = false;
    }
    task.get();
}
