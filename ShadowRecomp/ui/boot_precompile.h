#pragma once

struct BootPrecompile
{
    static inline bool s_isVisible = false;
    static void Init();
    static void Draw();
    static void Run();
};
