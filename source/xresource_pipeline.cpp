#include "Details/xresource_pipeline_compiler_base.cpp"
#include <iostream>

void displayProgressBar(const char* pTitle, float progress) noexcept
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    constexpr auto total_chars_v = 40;
    constexpr auto fill_progress = "========================================";
    constexpr auto empty_progress = "                                        ";
    constexpr auto bar_width = 35;
    const auto     pos = static_cast<int>(bar_width * progress);
    const auto     filled = total_chars_v - pos;
    const auto     empty = total_chars_v - (bar_width - pos);

    printf("\r[Info] %s: [%s>%s] %3d%%", pTitle, &fill_progress[filled], &empty_progress[empty], static_cast<int>(progress * 100.0));
    std::cout.flush();
}

