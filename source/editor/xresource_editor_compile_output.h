#ifndef XRESOURCE_EDITOR_COMPILE_OUTPUT_H
#define XRESOURCE_EDITOR_COMPILE_OUTPUT_H
#pragma once

// What a resource compiler printed, read into what a person wants to see: the warnings and errors (each with the lines that belong to it), and how far
// the compile is (its progress bar). Standalone, so a test can run it; the Compilation tab shows the result.
//
// The grammar is the one the pipeline prints (the same the Logs read: xlog::pipeline_output_adapter):
//   [Info] OUTPUT: D:\proj\Cache\...                 -> information (not listed)
//   [Warning] Mip count clamped to 12                -> a warning
//   [Error] Unsupported texture format 'exr'         -> an error
//     supported: png, dds                            -> a line with no tag belongs to the message above it
//   Title: [====>      ] 45%                         -> progress (the last one is how far the compile is)
//   [COMPILATION_SUCCESS]  /  ------------          -> end of a message
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xresource_editor::compilation
{
    enum class message_level : std::uint8_t { info, warning, error };

    struct output_message
    {
        message_level   m_Level = message_level::info;
        std::string     m_Text;                         // the first line, then the lines that belong to it
    };

    struct parsed_output
    {
        std::vector<output_message> m_Problems;         // warnings and errors, in the order they were printed
        int                         m_nWarnings = 0;
        int                         m_nErrors   = 0;
        bool                        m_bProgress = false;
        float                       m_Progress  = 0.0f; // 0..1 (the last progress line)
        std::string                 m_ProgressTitle;
    };

    namespace details
    {
        inline bool EqualNoCase(std::string_view A, std::string_view B) noexcept
        {
            if (A.size() != B.size()) return false;
            for (std::size_t i = 0; i < A.size(); ++i)
            {
                const char a = (A[i] >= 'A' && A[i] <= 'Z') ? static_cast<char>(A[i] + 32) : A[i];
                const char b = (B[i] >= 'A' && B[i] <= 'Z') ? static_cast<char>(B[i] + 32) : B[i];
                if (a != b) return false;
            }
            return true;
        }

        inline bool Blank(std::string_view Line) noexcept { return Line.find_first_not_of(" \t") == std::string_view::npos; }

        // "[Warning] text" -> the level and the text after the tag
        inline bool Tagged(std::string_view Line, message_level& Level, std::string_view& Text) noexcept
        {
            if (Line.size() < 6 || Line[0] != '[') return false;
            const auto Close = Line.find(']');
            if (Close == std::string_view::npos) return false;
            const auto Tag = Line.substr(1, Close - 1);
            if      (EqualNoCase(Tag, "Info"))    Level = message_level::info;
            else if (EqualNoCase(Tag, "Warning")) Level = message_level::warning;
            else if (EqualNoCase(Tag, "Error"))   Level = message_level::error;
            else return false;
            Text = Line.substr(Close + 1);
            if (!Text.empty() && Text.front() == ' ') Text.remove_prefix(1);
            return true;
        }

        // "Title: [====>      ] 45%"  (the bar is made of '=', '>' and spaces)
        inline bool Progress(std::string_view Line, float& Fraction, std::string& Title) noexcept
        {
            const auto Open = Line.find('[');
            if (Open == std::string_view::npos) return false;
            const auto Close = Line.find(']', Open);
            if (Close == std::string_view::npos || Close == Open + 1) return false;
            for (auto i = Open + 1; i < Close; ++i) if (Line[i] != '=' && Line[i] != '>' && Line[i] != ' ') return false;

            auto Rest = Line.substr(Close + 1);
            while (!Rest.empty() && Rest.front() == ' ') Rest.remove_prefix(1);
            int Percent = 0; std::size_t Digits = 0;
            while (Digits < Rest.size() && Rest[Digits] >= '0' && Rest[Digits] <= '9' && Digits < 4) Percent = Percent * 10 + (Rest[Digits++] - '0');
            if (Digits == 0 || Digits >= Rest.size() || Rest[Digits] != '%') return false;

            Fraction = Percent > 100 ? 1.0f : static_cast<float>(Percent) / 100.0f;
            auto Name = Line.substr(0, Open);
            while (!Name.empty() && (Name.back() == ' ' || Name.back() == ':')) Name.remove_suffix(1);
            Title.assign(Name);
            return true;
        }

        inline bool Separator(std::string_view Line) noexcept
        {
            return Line.size() >= 8 && Line.find_first_not_of("-=") == std::string_view::npos;
        }
    }

    inline parsed_output ParseOutput(std::string_view Output) noexcept
    {
        parsed_output Out;
        output_message Pending;
        bool bPending = false;

        auto Flush = [&]
        {
            if (bPending && Pending.m_Level != message_level::info)
            {
                (Pending.m_Level == message_level::error ? Out.m_nErrors : Out.m_nWarnings)++;
                Out.m_Problems.push_back(std::move(Pending));
            }
            Pending = {};
            bPending = false;
        };

        std::size_t Start = 0;
        while (Start <= Output.size())
        {
            auto Eol = Output.find('\n', Start);
            auto Line = Output.substr(Start, Eol == std::string_view::npos ? Output.size() - Start : Eol - Start);
            Start = Eol == std::string_view::npos ? Output.size() + 1 : Eol + 1;
            while (!Line.empty() && Line.back() == '\r') Line.remove_suffix(1);

            message_level Level; std::string_view Text;
            if (details::Tagged(Line, Level, Text))
            {
                Flush();
                // the progress bar may be printed under an [Info] tag
                if (float F; Level == message_level::info && details::Progress(Text, F, Out.m_ProgressTitle)) { Out.m_bProgress = true; Out.m_Progress = F; continue; }
                Pending.m_Level = Level; Pending.m_Text.assign(Text); bPending = true;
                continue;
            }
            if (Line == "[COMPILATION_SUCCESS]" || details::Separator(Line)) { Flush(); continue; }
            if (float F; details::Progress(Line, F, Out.m_ProgressTitle)) { Flush(); Out.m_bProgress = true; Out.m_Progress = F; continue; }
            if (details::Blank(Line)) continue;
            if (bPending) { Pending.m_Text += '\n'; Pending.m_Text.append(Line); continue; }
            // a line the compiler did not tag (the command line it was started with): information
        }
        Flush();
        return Out;
    }
}

#endif // XRESOURCE_EDITOR_COMPILE_OUTPUT_H
