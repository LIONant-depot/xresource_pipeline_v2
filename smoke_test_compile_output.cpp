// What the Compilation tab reads out of a compiler's output (source/editor/xresource_editor_compile_output.h). Standalone: built and run by
// source/Editors/LevelEditor/smoke/test_compile_output.py. Exit 0 when every check holds.
#include "source/editor/xresource_editor_compile_output.h"

#include <cstdio>
#include <cmath>

namespace
{
    int g_Failed = 0;

    void Check(bool bOk, const char* pWhat)
    {
        if (bOk) return;
        ++g_Failed;
        std::printf("FAIL: %s\n", pWhat);
    }
}

int main()
{
    using namespace xresource_editor::compilation;

    // warnings and errors are listed, information is not; the lines under a message belong to it
    {
        const auto P = ParseOutput(
            "\"C:\\tools\\compiler.exe\" -PROJECT x\n"
            "==============================\n"
            "[Info] OUTPUT: D:\\proj\\Cache\n"
            "[Warning] Mip count clamped to 12\n"
            "[Error] Unsupported texture format 'exr'\n"
            "  supported: png, dds\n"
            "  tried: exr\n"
            "[Info] done\n");
        Check(P.m_nWarnings == 1 && P.m_nErrors == 1 && P.m_Problems.size() == 2, "one warning and one error are listed, the information is not");
        Check(P.m_Problems.size() == 2 && P.m_Problems[0].m_Level == message_level::warning && P.m_Problems[0].m_Text == "Mip count clamped to 12", "the warning keeps its text, in the order printed");
        Check(P.m_Problems.size() == 2 && P.m_Problems[1].m_Text == "Unsupported texture format 'exr'\n  supported: png, dds\n  tried: exr", "the lines under the error belong to it");
        Check(!P.m_bProgress, "no progress in an output without a bar");
    }

    // the bar: the last one says how far the compile is, its title is what is printed before it
    {
        const auto P = ParseOutput("Compressing: [==>       ] 20%\nCompressing: [=======>  ] 78%\n");
        Check(P.m_bProgress && std::fabs(P.m_Progress - 0.78f) < 0.001f && P.m_ProgressTitle == "Compressing", "the last progress line is the progress, with its title");
        Check(P.m_Problems.empty(), "a progress line is not a message");
        const auto Q = ParseOutput("[Info] Mips: [====>     ] 45%\n");
        Check(Q.m_bProgress && std::fabs(Q.m_Progress - 0.45f) < 0.001f && Q.m_ProgressTitle == "Mips", "a bar printed under an [Info] tag is a bar too");
        const auto C = ParseOutput("X: [==========] 100%\n");
        Check(C.m_bProgress && C.m_Progress == 1.0f, "100% is a full bar");
        const auto N = ParseOutput("Some text [not a bar] 45%\nlist [1, 2, 3] 50%\n");
        Check(!N.m_bProgress, "brackets that are not a bar (and a percent) are not progress");
    }

    // a message ends at a separator, the compiler's own end word, or the next tag; a warning's lines do not run into what follows
    {
        const auto P = ParseOutput("[Warning] first\n----------------\nnot part of it\n[Warning] second\n[COMPILATION_SUCCESS]\nnor this\n");
        Check(P.m_Problems.size() == 2 && P.m_Problems[0].m_Text == "first" && P.m_Problems[1].m_Text == "second", "separators and the end word close a message");
    }

    // tags in any case (the console processing looks for [warning] without caring), Windows line ends, blank lines, and an output that is empty or has no end of line
    {
        const auto P = ParseOutput("[WARNING] shout\r\n\r\n[error] quiet\r\n");
        Check(P.m_nWarnings == 1 && P.m_nErrors == 1, "tags are read in any case, with \\r\\n");
        Check(P.m_Problems.size() == 2 && P.m_Problems[1].m_Text == "quiet", "and the \\r is not part of the text");
        const auto E = ParseOutput("");
        Check(E.m_Problems.empty() && !E.m_bProgress, "an empty output has nothing");
        const auto T = ParseOutput("[Error] last line, no end of line");
        Check(T.m_nErrors == 1, "the last line counts without an end of line");
    }

    if (g_Failed == 0) std::printf("all checks passed\n");
    return g_Failed == 0 ? 0 : 1;
}
