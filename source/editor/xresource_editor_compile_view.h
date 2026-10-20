#ifndef XRESOURCE_EDITOR_COMPILE_VIEW_H
#define XRESOURCE_EDITOR_COMPILE_VIEW_H
#pragma once

// How a compile looks, in one place: the Compilation tab and the Compile bar of every resource editor draw with these, so they read as one system.
// The look is muted, with colour only where it means something (the same colours and glyph shapes as the Logs): a state as a chip, a warning as an amber
// triangle, an error as a red circle, and a progress bar for what the compiler said it did.
#include "xresource_editor_asset_mgr.h"
#include "xresource_editor_compile_output.h"
#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>

namespace xresource_editor::compile_ui
{
    inline ImU32 ColError() noexcept { return IM_COL32(212, 107, 105, 255); }
    inline ImU32 ColWarn()  noexcept { return IM_COL32(201, 160,  74, 255); }
    inline ImU32 ColOk()    noexcept { return IM_COL32(112, 176, 124, 255); }
    inline ImU32 ColRun()   noexcept { return IM_COL32( 92, 160, 232, 255); }
    inline ImU32 ColMuted() noexcept { return IM_COL32(140, 140, 140, 255); }
    inline ImU32 Fade(ImU32 C, int A) noexcept { return (C & 0x00FFFFFFu) | (static_cast<ImU32>(A) << 24); }

    // The glyphs of the Logs: a filled circle is an error, a triangle a warning
    inline void DrawGlyph(ImDrawList* pList, ImVec2 C, bool bError) noexcept
    {
        if (bError) pList->AddCircleFilled(C, 4.5f, ColError());
        else        pList->AddTriangleFilled(ImVec2(C.x, C.y - 5.0f), ImVec2(C.x + 5.0f, C.y + 4.0f), ImVec2(C.x - 5.0f, C.y + 4.0f), ColWarn());
    }

    // A smile: all went well (a ring, two eyes and a mouth, drawn by lines so it is sharp at any size)
    inline void DrawSmile(ImDrawList* pList, ImVec2 C, ImU32 Col) noexcept
    {
        pList->AddCircle(C, 7.0f, Col, 0, 1.4f);
        pList->AddCircleFilled(ImVec2(C.x - 2.4f, C.y - 2.0f), 1.0f, Col);
        pList->AddCircleFilled(ImVec2(C.x + 2.4f, C.y - 2.0f), 1.0f, Col);
        pList->PathClear();
        pList->PathArcTo(ImVec2(C.x, C.y + 0.2f), 3.9f, 0.45f, 3.14159f - 0.45f, 12);      // the lower half: a mouth that curves up at its ends
        pList->PathStroke(Col, 0, 1.4f);
    }

    // A small ring that turns: something is happening
    inline void Spinner(ImDrawList* pList, ImVec2 C, float R, ImU32 Col) noexcept
    {
        const float T = static_cast<float>(ImGui::GetTime()) * 5.0f;
        pList->PathClear();
        pList->PathArcTo(C, R, T, T + 4.4f, 20);
        pList->PathStroke(Col, 0, 2.0f);
    }

    // A pill with a word in it, laid out like text (it takes its place in the row)
    inline void Chip(const char* pText, ImU32 Col, bool bSpin = false) noexcept
    {
        const ImVec2 Text = ImGui::CalcTextSize(pText);
        const float  Pad  = 8.0f, Spin = bSpin ? 16.0f : 0.0f;
        const ImVec2 Size(Text.x + Pad * 2 + Spin, Text.y + 4.0f);
        const ImVec2 P = ImGui::GetCursorScreenPos();
        auto* pList = ImGui::GetWindowDrawList();
        pList->AddRectFilled(P, ImVec2(P.x + Size.x, P.y + Size.y), Fade(Col, 38), Size.y * 0.5f);
        pList->AddRect(P, ImVec2(P.x + Size.x, P.y + Size.y), Fade(Col, 110), Size.y * 0.5f);
        if (bSpin) Spinner(pList, ImVec2(P.x + Pad + 4.0f, P.y + Size.y * 0.5f), 4.0f, Col);
        pList->AddText(ImVec2(P.x + Pad + Spin, P.y + 2.0f), Col, pText);
        ImGui::Dummy(Size);
    }

    // The width Chip takes
    inline float ChipWidth(const char* pText, bool bSpin = false) noexcept { return ImGui::CalcTextSize(pText).x + 16.0f + (bSpin ? 16.0f : 0.0f); }

    // A count with its glyph ("▲ 2"), laid out like text; nothing for zero
    inline float BadgeWidth(int N) noexcept { return N ? 18.0f + ImGui::CalcTextSize(std::format("{}", N).c_str()).x + 8.0f : 0.0f; }
    inline void Badge(bool bError, int N) noexcept
    {
        if (!N) return;
        const ImVec2 C = ImGui::GetCursorScreenPos();
        DrawGlyph(ImGui::GetWindowDrawList(), ImVec2(C.x + 7.0f, C.y + ImGui::GetTextLineHeight() * 0.5f + 2.0f), bError);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(bError ? ColError() : ColWarn()));
        ImGui::Text("%d", N);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 8.0f);
    }

    inline std::string FormatSeconds(double S)
    {
        if (S < 1.0)    return std::format("{:.0f} ms", S * 1000.0);
        if (S < 60.0)   return std::format("{:.1f} s", S);
        return std::format("{}m {:02}s", static_cast<int>(S) / 60, static_cast<int>(S) % 60);
    }

    //=============================================================================

    // What the compile of one resource is, read out of its log: the state, the times, and what its output says. The output is read again only when it grew.
    struct log_view
    {
        compilation::historical_entry::result   m_Result   = compilation::historical_entry::result::QUEUED;
        bool                                    m_bRunning = false;
        bool                                    m_bQueued  = false;
        std::uint64_t                           m_Order    = 0;
        double                                  m_Seconds  = -1.0;      // how long it ran (or has run, or waited); negative: unknown
        const compilation::parsed_output*       m_pParsed  = nullptr;   // valid until the next ReadLog of another log... see below
        std::string                             m_Raw;                  // the whole output (only taken when asked)
    };

    struct parse_cache
    {
        std::weak_ptr<compilation::historical_entry::log>   m_Owner;        // the log it was read from (an address can be used by another log later)
        std::size_t                                         m_Len = static_cast<std::size_t>(-1);
        compilation::parsed_output                          m_Parsed;
    };

    // The reads of the logs by whoever draws (the interface thread): what a log said is parsed again only when it grew. The pointer a view holds stays valid:
    // elements of the map do not move.
    inline std::unordered_map<const void*, parse_cache>& ParseCache() noexcept { static std::unordered_map<const void*, parse_cache> Cache; return Cache; }

    inline void PruneParseCache() noexcept
    {
        auto& Cache = ParseCache();
        if (Cache.size() > 512) std::erase_if(Cache, [](const auto& KV) { return KV.second.m_Owner.expired(); });
    }

    inline log_view ReadLog( const std::shared_ptr<compilation::historical_entry::log>& Log, bool bWantRaw = false )
    {
        using result = compilation::historical_entry::result;
        log_view V;
        if (!Log) return V;

        auto& Cache = ParseCache()[Log.get()];
        if (Cache.m_Owner.lock() != Log) { Cache = {}; Cache.m_Owner = Log; }

        std::string Copy;
        std::chrono::steady_clock::time_point Queued, Begin, End;
        std::size_t Len = 0;
        {
            xcontainer::lock::scope lock(*Log);
            auto& L = Log->get();
            V.m_Result = L.m_Result;
            V.m_Order  = L.m_Order;
            Queued = L.m_Queued; Begin = L.m_Begin; End = L.m_End;
            Len = L.m_Log.size();
            if (Cache.m_Len != Len || bWantRaw) Copy = L.m_Log;       // the output is copied (and read again) only when it grew, or when it is shown
        }
        if (Cache.m_Len != Len) { Cache.m_Parsed = compilation::ParseOutput(Copy); Cache.m_Len = Len; }
        V.m_pParsed = &Cache.m_Parsed;
        if (bWantRaw) V.m_Raw = std::move(Copy);

        V.m_bRunning = V.m_Result == result::COMPILING || V.m_Result == result::COMPILING_WARNINGS;
        V.m_bQueued  = V.m_Result == result::QUEUED;
        const auto Now  = std::chrono::steady_clock::now();
        const auto Zero = std::chrono::steady_clock::time_point{};
        if      (V.m_bQueued && Queued != Zero)                       V.m_Seconds = std::chrono::duration<double>(Now - Queued).count();
        else if (V.m_bRunning && Begin != Zero)                       V.m_Seconds = std::chrono::duration<double>(Now - Begin).count();
        else if (Begin != Zero && End != Zero && End >= Begin)        V.m_Seconds = std::chrono::duration<double>(End - Begin).count();
        return V;
    }

    // The state as a word and a colour
    inline void StateOf( compilation::historical_entry::result Result, const char*& pWord, ImU32& Col, bool& bSpin ) noexcept
    {
        using result = compilation::historical_entry::result;
        bSpin = false;
        switch (Result)
        {
        case result::QUEUED:             pWord = "Queued";               Col = ColMuted(); break;
        case result::COMPILING:          pWord = "Compiling";            Col = ColRun();   bSpin = true; break;
        case result::COMPILING_WARNINGS: pWord = "Compiling";            Col = ColWarn();  bSpin = true; break;
        case result::SUCCESS:            pWord = "Done";                 Col = ColOk();    break;
        case result::SUCCESS_WARNINGS:   pWord = "Done with warnings";   Col = ColWarn();  break;
        case result::FAILURE:            pWord = "Failed";               Col = ColError(); break;
        case result::CANCELLED:          pWord = "Cancelled";            Col = ColMuted(); break;
        default:                         pWord = "?";                    Col = ColMuted(); break;
        }
    }

    // The progress of a compile as a bar: what the compiler said it did, or a bar that moves while it only says it works. Text: drawn over the bar (empty: none).
    inline void ProgressBar( const log_view& V, ImVec2 Size, const char* pOverlay = "" ) noexcept
    {
        using result = compilation::historical_entry::result;
        const ImU32 Col = V.m_Result == result::COMPILING_WARNINGS ? ColWarn() : ColRun();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(Col));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.07f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Size.y * 0.5f);
        const float F = V.m_pParsed && V.m_pParsed->m_bProgress ? V.m_pParsed->m_Progress : (V.m_Result == result::SUCCESS || V.m_Result == result::SUCCESS_WARNINGS ? 1.0f : -1.0f * static_cast<float>(ImGui::GetTime()));
        ImGui::ProgressBar(F, Size, pOverlay);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    // "Compression  45%" or "Working..." : what the compiler is doing
    inline std::string ProgressText( const log_view& V )
    {
        if (V.m_pParsed && V.m_pParsed->m_bProgress)
            return std::format("{}  {}%", V.m_pParsed->m_ProgressTitle.empty() ? "Working" : V.m_pParsed->m_ProgressTitle, static_cast<int>(V.m_pParsed->m_Progress * 100.0f + 0.5f));
        return "Working...";
    }
}

#endif // XRESOURCE_EDITOR_COMPILE_VIEW_H
