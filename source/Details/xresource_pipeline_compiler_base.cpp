
#include "xresource_pipeline.h"
#include <cwctype>
#include <stdio.h>
#include <filesystem>
#include "dependencies/xcmdline/source/xcmdline_parser.h"

namespace xresource_pipeline::compiler {

base* g_pBase = nullptr;

//---------------------------------------------------------------------------------------

void LogFunction( msg_type Type, std::string_view String, std::uint32_t Line, std::string_view file ) noexcept
{
    if (String.empty()) String = "Unkown";

    //
    // Print to the standard output
    //
    {
        static bool bPendingNewLine = false;
        if (String[0] == '\r')
        {
            std::cout << std::format
            ("\r[{}] {}"
            , msg_type::INFO == Type ? "Info" : msg_type::WARNING == Type ? "Warning" : "Error"
            , String.substr(1)
            );
            bPendingNewLine = true;
        }
        else
        {
            // In case of surprised warning or errors while doing pertentages we need to make sure we print a new line
            if (bPendingNewLine) printf("\n");
            bPendingNewLine = false;

            std::cout << std::format
            ("[{}] {}\n"
            , msg_type::INFO == Type ? "Info" : msg_type::WARNING == Type ? "Warning" : "Error"
            , String
            );
        }

        // Make sure we always print the message
        std::fflush(stdout);
    }

    //
    // Save to the log file
    //
    if( g_pBase->m_pLogFile ) 
    {
        auto OutputString = std::format( "{}({}) [{}] {}\n"
            , file.empty() ? std::string_view("Unkown file") : file
            , Line
            , msg_type::INFO    == Type ? "Info"
            : msg_type::WARNING == Type ? "Warning"
            : "Error"
            , String[0] == '\r' ? String.substr(1) : String
            );

        if( auto Err = fprintf_s(g_pBase->m_pLogFile, "%s", OutputString.c_str() ); Err < 0 )
        {
            printf( "Fail to save data in log\n" );
        }

        // Make sure things are written in time...
        fflush(g_pBase->m_pLogFile);
    }
}

//--------------------------------------------------------------------------

void xerr_ErrorHandler(const char* StateStr, std::uint8_t state, std::string_view msg, std::uint32_t line, std::string_view file)
{
    // If the user ask us to give all the info... we will...
    if (g_pBase && g_pBase->m_DebugType == base::debug_type::Dz ) LogFunction(msg_type::ERROR, std::format( "{}; with ErrCode({}) = {}.", msg, state, StateStr ), line, file);
}

//--------------------------------------------------------------------------

base::base( void ) noexcept
{
    g_pBase = this;
    xerr::m_pCallback = xerr_ErrorHandler;
}

//--------------------------------------------------------------------------
base::~base()
{
    //
    // Make sure that the log file is closed
    //
    if (m_pLogFile) fclose(m_pLogFile);
}

//------------------------------------------------------------------------------
// https://superuser.com/questions/1362080/which-characters-are-invalid-for-an-ms-dos-filename
// Try to create a very compact resource file name
//------------------------------------------------------------------------------
/*
template< typename T_CHAR, int extra_v=0, typename T > constexpr
std::string CompactName( T aVal ) noexcept
{
    static_assert(std::is_integral_v<T>);
    using t = to_uint_t<T>;
    using u = xcore::string::ref<T_CHAR>::units;
    constexpr auto  string_capacity_v   = 64;
    auto            String              = xcore::string::ref<T_CHAR>(u(string_capacity_v));
    int             iCursor             = 0;
    auto            Val                 = static_cast<t>(aVal);
    constexpr auto  valid_chars_v       = []()consteval 
    {
        std::array<T_CHAR, 127*extra_v + sizeof("0123456789ABCDEFGHINKLMNOPQRSTUVWXYZ !#$%&'()-@^_`{}~")> Array{};
        int s;
        if constexpr (sizeof(T_CHAR) == 1)      for(s = 0; Array.data()[s] =  "0123456789ABCDEFGHINKLMNOPQRSTUVWXYZ !#$%&'()-@^_`{}~"[s]; ++s);
        else if constexpr (sizeof(T_CHAR) == 2) for(s = 0; Array.data()[s] = L"0123456789ABCDEFGHINKLMNOPQRSTUVWXYZ !#$%&'()-@^_`{}~"[s]; ++s);
        else
        {
            xassert(false);
        }

        for( int i=0; i<(127*extra_v); ++i )
        {
            Array[ i + s ] = 128 + i;
        }

        return Array;
    }();
    constexpr auto base_v               = valid_chars_v.size();
    do
    {
        const auto CVal = t(Val % base_v);
        Val /= base_v;
        String[u(iCursor++)] = valid_chars_v[CVal];

    } while (Val > 0 && iCursor < string_capacity_v);

    //  terminate string; 
    String[u(iCursor)] = 0;

    return String;
}
*/

//--------------------------------------------------------------------------

xerr base::setupPaths( void ) noexcept
{
    //
    // Get resource partial path should be something like this: "ResourceType/0F/23/321230F23"
    //
    {
        std::size_t iStartString;
        std::size_t iEndString;
        if( xstrtool::findI( m_InputSrcDescriptorPath, L"Descriptors") != std::string::npos )
        {
            iStartString = xstrtool::findI( m_InputSrcDescriptorPath, L"/");
        }
        else
        {
            if (xstrtool::findI(m_InputSrcDescriptorPath, L"Cache") != std::string::npos ) 
                return xerr::create_f<state, "Expecting a descriptor path that starts with a 'C' for Cache or 'D' for Descriptor" >();

            iStartString = xstrtool::findI(m_InputSrcDescriptorPath, L"/");
            if (iStartString != std::string::npos ) iStartString = xstrtool::findI(m_InputSrcDescriptorPath, L"/", iStartString + 1);
        }
        iEndString = xstrtool::rfindI(m_InputSrcDescriptorPath, L".");
        if (iEndString == std::string::npos )  
            return xerr::create_f<state, "Expecting a descriptor path that has an extension '.'">();

        if (iStartString == std::string::npos ) 
            return xerr::create_f<state, "The descriptor did not included a valid path (1)">();

        // Now that should have something that looks like this: "ResourceType/0F/23/321230F23"
        m_ResourcePartialPath = xstrtool::CopyN( std::wstring_view( m_InputSrcDescriptorPath.begin() + iStartString+1, m_InputSrcDescriptorPath.end()), iEndString - iStartString - 1);
    }

    {
        auto GUIDInstance = xstrtool::To(m_ResourcePartialPath.substr( 1+m_ResourcePartialPath.rfind(L"/") ));
        auto Result       = std::from_chars(GUIDInstance.data(), GUIDInstance.data() + GUIDInstance.size(), m_ResourceGuid.m_Value, 16);

        if (Result.ec != std::errc())
            return xerr::create_f<state, "Fail to convert the resource path into a regular GUID">();
    }

    //
    // Get the resource type string
    //
    {
        std::size_t iLastCharOfTheType = xstrtool::findI(m_ResourcePartialPath, L"/");
        if (iLastCharOfTheType == std::string::npos ) 
            return xerr::create_f< state, "The descriptor did not included a valid path (2)">();

        m_ResourceType = xstrtool::CopyN( m_ResourcePartialPath, iLastCharOfTheType);
    }

    //
    // Set all the core directories
    //
    m_ProjectPaths.m_Descriptors        = std::format(L"{}/Descriptors",  m_ProjectPaths.m_Project);
    m_ProjectPaths.m_Config             = std::format(L"{}/Config",       m_ProjectPaths.m_Project);
    m_ProjectPaths.m_Assets             = std::format(L"{}/Assets",       m_ProjectPaths.m_Project);
    m_ProjectPaths.m_Cache              = std::format(L"{}/Cache",        m_ProjectPaths.m_Project);
    m_ProjectPaths.m_CacheTemp          = std::format(L"{}/Temp",         m_ProjectPaths.m_Cache);
    m_ProjectPaths.m_CachedDescriptors  = std::format(L"{}/Descriptors",  m_ProjectPaths.m_Cache);
    m_ProjectPaths.m_Resources          = std::format(L"{}/Resources",    m_ProjectPaths.m_Cache);
    m_ProjectPaths.m_ResourcesPlatforms = std::format(L"{}/Platforms",    m_ProjectPaths.m_Resources);
    m_ProjectPaths.m_ResourcesLogs      = std::format(L"{}/Logs",         m_ProjectPaths.m_Resources);

    //
    // Set up the path required for this complilation
    //
    m_ResourceLogPath = std::format(L"{}/{}.log", m_ProjectPaths.m_ResourcesLogs, m_ResourcePartialPath );

    // Make sure the log path is ready
    if (auto Err = base::CreatePath(m_ResourceLogPath); Err)
        return Err;

    // Open the log file
    if ( errno_t Err = _wfopen_s( &m_pLogFile, std::format(L"{}/Log.txt", m_ResourceLogPath ).c_str(), L"wt"); Err != 0)
    {
        std::array<wchar_t, 256> errMsg;
        if (_wcserror_s( errMsg.data(), errMsg.size(), Err) == 0)
        {
            xerr::LogMessage<state::FAILURE>(std::format("Log file File system Error: {}, {}", Err, xstrtool::To(errMsg.data())));
        }
        else 
        {
            xerr::LogMessage<state::FAILURE>(std::format("Log file File system unkown error: {}", Err));
        }

        return xerr::create_f<state, "Fail to create the log file">();
    }
        

    //
    // Make sure all the required directories are created
    //
    if (auto Err = base::CreatePath(m_ProjectPaths.m_Cache); Err) return Err;
    if (auto Err = base::CreatePath(m_ProjectPaths.m_CacheTemp); Err) return Err;
    if (auto Err = base::CreatePath(m_ProjectPaths.m_CachedDescriptors); Err) return Err;
    if (auto Err = base::CreatePath(m_ProjectPaths.m_Resources); Err) return Err;
    if (auto Err = base::CreatePath(m_ProjectPaths.m_ResourcesPlatforms); Err) return Err;
    if (auto Err = base::CreatePath(m_ProjectPaths.m_ResourcesLogs); Err) return Err;

    //
    // Set all the valid target paths
    //
    {
          std::wstring EverythingExceptTheResourceGUID;

          auto iLast = xstrtool::rfindI(m_ResourcePartialPath, L"/");
          if (iLast == std::string::npos ) 
            return xerr::create_f<state, "The descriptor did not included a valid path (3)">();

          EverythingExceptTheResourceGUID = xstrtool::CopyN(m_ResourcePartialPath, iLast);

        for (auto& E : m_Target)
        {
            if (E.m_bValid)
            {
                // This should end up like... "FullPath/Project.lion_project/Cache/Platforms/Windows/ResourceType/0F/23"
                E.m_DataPath = std::format(L"{}/{}/{}"
                    , m_ProjectPaths.m_ResourcesPlatforms
                    , wplatform_v[static_cast<int>(E.m_Platform)]
                    , EverythingExceptTheResourceGUID
                    );

                // Make sure is ready for rock and roll
                if (auto Err = base::CreatePath(E.m_DataPath); Err)
                    return Err;

                // OK then now everything including the final file name (no extension)
                // This should end up like... "FullPath/Project.lion_project/Cache/Platforms/Windows/ResourceType/0F/23/4fdsdf230F"
                E.m_DataPath = std::format(L"{}/{}/{}"
                    , m_ProjectPaths.m_ResourcesPlatforms
                    , wplatform_v[static_cast<int>(E.m_Platform)]
                    , m_ResourcePartialPath
                );

            }
        }
    }

    return {};
}

//--------------------------------------------------------------------------

void base::LogMessage(xresource_pipeline::msg_type Type, const char* pString, const std::source_location Loc ) const noexcept
{
    LogFunction(Type, pString, Loc.line(), Loc.file_name());
}

//--------------------------------------------------------------------------

void base::LogMessage(xresource_pipeline::msg_type Type, std::string_view String, const std::source_location Loc) const noexcept
{
    LogFunction(Type, String, Loc.line(), Loc.file_name());
}

//--------------------------------------------------------------------------

void base::LogMessage(xresource_pipeline::msg_type Type, std::string&& String, const std::source_location Loc ) const noexcept
{
    LogFunction( Type, String, Loc.line(), Loc.file_name() );
}

//--------------------------------------------------------------------------

void base::LogMessage(xresource_pipeline::msg_type Type, std::wstring&& String, const std::source_location Loc) const noexcept
{
    LogFunction(Type, xstrtool::To(String), Loc.line(), Loc.file_name());
}

//--------------------------------------------------------------------------

xerr base::InternalParse( const int argc, const char *argv[] )
{
    xcmdline::parser CmdLineParser;
    
    //
    // Create the switches and their rules
    //
    auto MainGroup      = CmdLineParser.addGroup( "Non-Optionals", "These are all the commands that you must always pass to the compiler");
    auto OptiminalGroup = CmdLineParser.addGroup( "Optionals",     "These are commands that have default so you may choose not to include in the command line");

    const auto hHelp = CmdLineParser.addOption
        ( "H"
        , "Asks the system to print the help.\n"
        , false
        , 0
        , OptiminalGroup
        );

    const auto hOptimization = CmdLineParser.addOption
        ( "OPTIMIZATION"
        , "Tells the compiler how much it should optimize the resource:\n"
             "   O0 - Compile as fast as possible\n"
             "   Q1 - Compile with optimizations (Default)\n"
             "   Qz - Maximum performance for asset"
        , false
        , 1
        , OptiminalGroup
        );

    const auto hDebug = CmdLineParser.addOption
        ( "DEBUG"
        , "Tells the compiler to help debug the resource:\n"
            "   D0 - Basic debug information (Default)\n"
            "   D1 - Extra debug information\n"
            "   Dz - Maximum debug information"
        , false
        , 1
        , OptiminalGroup
        );

    const auto hTarget = CmdLineParser.addOption
        ( "TARGET"
        , "Tells which platforms should be compiling for,\n"
            "   use one or multiple of the following:\n"
            "   WINDOWS MAC IOS LINUX ANDROID\n"
            "   WINDOWS (is the default)"
        , false
        , 1
        , OptiminalGroup
        );

    const auto hProject =  CmdLineParser.addOption
        ( "PROJECT"
        , "This tells the compiler for which project we are working for:\n"
            "   Many of the paths are relative to the project forlder so this is a must have.\n"
            "   This is the full path of to the project folder\n"
            "   <FullPathToProjectFolder>\n"
        , true
        , 1
        , MainGroup 
        );

    const auto hDescriptor = CmdLineParser.addOption
        ( "DESCRIPTOR"
        , "Tells the which descriptor we should be compiling:\n"
            "   Path Relative to project folder to the actual descriptor folder\n"
            "   Make sure you use quotes to make sure everything is safe\n"
            "   <FromProjectToDescriptor.desc>\n"
        , true
        , 1
        , MainGroup 
        );

    const auto hOutput = CmdLineParser.addOption
        ( "OUTPUT"
        , "Tells the compiler where the final resource should be saved:\n"
            "   This path should be a full path and it does not include the platform\n"
            "   The system will add the platform, type, and guids the the final path\n"
            "   <FullPathOfTheFinalResource>\n"
        , true
        , 1
        , MainGroup 
        );
    
    //
    // Start parsing the arguments
    //
    if( auto Err =  CmdLineParser.Parse( argc, argv ); Err  )
        return Err;
    
    //
    // Go through the parameters of the command line
    //
    if (CmdLineParser.hasOption(hHelp))
    {
        CmdLineParser.printHelp();
        return xerr::create<state::DISPLAY_HELP, "User requested to print help">();
    }

    //
    // Targets
    //
    if (CmdLineParser.hasOption(hTarget))
    {
        if ( auto result = CmdLineParser.getOptionArgAs<std::string>(hTarget, 0); std::holds_alternative<xerr>(result) )
            return std::get<xerr>(result);

        const auto Count = CmdLineParser.getOptionArgCount(hTarget);
        for (int i = 0; i < Count; i++)
        {
            bool         bFound = false;
            std::wstring String;

            if ( auto Param = CmdLineParser.getOptionArgAs<std::string>(hTarget, i); std::holds_alternative<xerr>(Param) )
                return std::get<xerr>(Param);
            else
                String = xstrtool::To(std::move(std::get<std::string>(Param)));

            // Go through all the platforms and pick up the right one
            constexpr auto TargetCount = static_cast<int>(xresource_pipeline::platform::COUNT);
            for (int ip = 0; ip < TargetCount; ip++)
            {
                if ( xstrtool::CompareI( xresource_pipeline::wplatform_v[ip], String ) == 0 )
                {
                    platform& NewTarget = m_Target[ip];

                    if (NewTarget.m_bValid)
                        return xerr::create_f<state, "The same platform was enter multiple times">();

                    NewTarget.m_Platform = static_cast<xresource_pipeline::platform>(ip);
                    NewTarget.m_bValid   = true;

                    bFound = true;
                    break;
                }
            }

            if (bFound == false)
                return xerr::create_f<state, "Platform not supported">();
        }
    }
    else
    {
        m_Target[static_cast<int>(xresource_pipeline::platform::WINDOWS)].m_bValid = true;
    }
    
    //
    // Project
    //
    if (CmdLineParser.hasOption(hProject))
    {
        if (auto result = CmdLineParser.getOptionArgAs<std::string>(hProject, 0); std::holds_alternative<xerr>(result))
            return std::get<xerr>(result);
        else
        {
            if ( xstrtool::findI(std::get<std::string>(result), ".lion_project") == std::string::npos 
              && xstrtool::findI(std::get<std::string>(result), ".lion_library") == std::string::npos )
            {
                return xerr::create_f<state, "I got a path in the PROJECT switch that is not a .lion_project or .lion_library path">();
            }

            m_ProjectPaths.m_Project = xstrtool::To(std::get<std::string>(result));
            xstrtool::PathNormalize(m_ProjectPaths.m_Project);
        }
    }
    assert(!m_ProjectPaths.m_Project.empty());

    //
    // Output
    //
    if (CmdLineParser.hasOption(hOutput))
    {
        if (auto result = CmdLineParser.getOptionArgAs<std::string>(hOutput, 0); std::holds_alternative<xerr>(result))
            return std::get<xerr>(result);
        else
        {
            m_ProjectPaths.m_Output = xstrtool::To(std::get<std::string>(result));
            xstrtool::PathNormalize(m_ProjectPaths.m_Output);
        }
    }

    //
    // Descriptor
    //
    if (CmdLineParser.hasOption(hDescriptor))
    {
        if (auto result = CmdLineParser.getOptionArgAs<std::string>(hDescriptor, 0); std::holds_alternative<xerr>(result))
            return std::get<xerr>(result);
        else
        {
            m_InputSrcDescriptorPath = xstrtool::To(std::get<std::string>(result));

            if (xstrtool::findI(m_InputSrcDescriptorPath, L".desc") == std::string::npos )
                return xerr::create_f<state, "You must specify the descriptor path which includes the extension">();

            xstrtool::PathNormalize(m_InputSrcDescriptorPath);
        }
    }

    //
    // Optimization
    //
    m_OptimizationType = optimization_type::O1;
    if (CmdLineParser.hasOption(hOptimization))
    {
        if (auto result = CmdLineParser.getOptionArgAs<std::string>(hOptimization, 0); std::holds_alternative<xerr>(result))
            return std::get<xerr>(result);
        else
        {
            std::string Type = std::get<std::string>(result);

            if (xstrtool::CompareI(Type, std::string_view("O0")) == 0)
            {
                m_OptimizationType = optimization_type::O0;
            }
            else if (xstrtool::CompareI(Type, std::string_view("O1")) == 0)
            {
                m_OptimizationType = optimization_type::O1;
            }
            else if (xstrtool::CompareI(Type,std::string_view("Oz")) == 0)
            {
                m_OptimizationType = optimization_type::O1;
            }
            else
            {
                return xerr::create_f<state, "Optimization Type not supported">();
            }
        }
    }

    //
    // Debug
    //
    m_DebugType = debug_type::D0;
    if (CmdLineParser.hasOption(hDebug))
    {
        if (auto result = CmdLineParser.getOptionArgAs<std::string>(hDebug, 0); std::holds_alternative<xerr>(result))
            return std::get<xerr>(result);
        else
        {
            std::string Type = std::get<std::string>(result);

            if (xstrtool::CompareI(Type, std::string_view("D0")) == 0)
            {
                m_DebugType = debug_type::D0;
            }
            else if (xstrtool::CompareI(Type, std::string_view("D1")) == 0)
            {
                m_DebugType = debug_type::D1;
            }
            else if (xstrtool::CompareI(Type, std::string_view("Dz")) == 0)
            {
                m_DebugType = debug_type::Dz;
            }
            else
            {
                return xerr::create_f<state, "Debug Type not supported">();
            }
        }
    }

    //
    // Logs data base
    //
    if (auto Err = setupPaths(); Err)
        return Err;

    //
    // Let the user know about the command line options
    //
    if (m_DebugType == debug_type::D1 || m_DebugType == debug_type::Dz)
    {
        LogMessage(msg_type::INFO, "Compiler command line options" );
        LogMessage(msg_type::INFO, std::format(L"PROJECT: {}", m_ProjectPaths.m_Project));

        {
            std::wstring TargetList;
            for (auto& E : m_Target)
            {
                if (E.m_bValid)
                {
                    TargetList += xresource_pipeline::wplatform_v[static_cast<int>(E.m_Platform)];
                    TargetList += L" ";
                }
            }
            LogMessage(msg_type::INFO, std::format(L"Targets: {}", TargetList));
        }

        LogMessage(msg_type::INFO, std::format(L"OUTPUT: {}", m_ProjectPaths.m_Output));
        LogMessage(msg_type::INFO, std::format(L"DESCRIPTOR: {}", m_InputSrcDescriptorPath));
        LogMessage(msg_type::INFO, std::format(L"OPTIMIZATION: {}", m_OptimizationType == optimization_type::O0 ? L"O0" : m_OptimizationType == optimization_type::O1 ? L"O1" : L"Oz"));
        LogMessage(msg_type::INFO, std::format(L"DEBUG: {}", m_DebugType == debug_type::D0 ? L"D0" : m_DebugType == debug_type::D1 ? L"D1" : L"Dz"));
    }

    return {};
}

//--------------------------------------------------------------------------

const std::wstring_view base::getDestinationPath( xresource_pipeline::platform p ) const noexcept
{
    const int Index = static_cast<int>(p);
    assert( m_Target[Index].m_bValid );
    assert( m_Target[Index].m_Platform == p );
    return m_Target[Index].m_DataPath;
}

//--------------------------------------------------------------------------

xerr base::Parse( int argc, const char *argv[] ) noexcept
{
    try
    {
        if( auto Err = InternalParse( argc, argv ); Err )
        {
            if( Err.getState<state>() == state::DISPLAY_HELP )
            {
                Err.clear();
            }
            else
            {
                LogMessage( msg_type::ERROR, Err.getMessage() );
            }
            
            return Err;
        }
    }
    catch (std::runtime_error RunTimeError)
    {
        LogMessage(msg_type::ERROR, std::format("{}", RunTimeError.what()));
        return xerr::create_f<state, "FAILED: exception thrown exiting...">();
    }

    return {};
}

//--------------------------------------------------------------------------

xerr base::CreatePath( const std::wstring_view Path ) const noexcept
{
    std::error_code         ec;
    std::filesystem:: path   path{ Path };

    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        LogMessage(msg_type::ERROR, std::format("Fail to create a directory [{}] with error [{}]", xstrtool::To(Path), ec.message() ));
        return xerr::create_f<state,"Fail to create a directory">();
    }

    return {};
}

//--------------------------------------------------------------------------

void base::displayProgressBar(const char* pTitle, float progress) const noexcept
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    constexpr auto total_chars_v    = 40;
    constexpr auto fill_progress    = "========================================";
    constexpr auto empty_progress   = "                                        ";
    constexpr auto bar_width        = 35;
    const auto     pos              = static_cast<int>(bar_width * progress);
    const auto     filled           = total_chars_v - pos;
    const auto     empty            = total_chars_v - (bar_width - pos);

    LogMessage( msg_type::INFO, std::format("\r{}: [{}>{}] {:3}%", pTitle, &fill_progress[filled], &empty_progress[empty], static_cast<int>(progress * 100.0)) );
    if (progress >= 1) printf("\n");
}


//--------------------------------------------------------------------------

xerr base::Compile( void ) noexcept
{
    m_Timmer = std::chrono::steady_clock::now();

    //
    // Create the log folder
    //
    /*
    {
        std::error_code         ec;
        std::filesystem::path   LogPath { xcore::string::To<wchar_t>(m_BrowserPath).data() };

        std::filesystem::create_directories( LogPath, ec );
        if( ec )
        {
            XLOG_CHANNEL_ERROR( m_LogChannel, "Fail to create the log directory [%s]", ec.message().c_str() );
            return xerr_failure_s( "Fail to create the log directory" );
        }
    }
    */
    //
    // Create log file per platform
    //
    /*
    for( auto& Entry : m_Target )
    {
        if( Entry.m_bValid == false )
            continue;

        const auto  p           = x_static_cast<x_target::platform>( m_Target.getIndexByEntry(Entry) );

        xstring Path;
        Path.setup( "%s/%s.txt"
                    , m_LogsPath.getPtr()
                    , x_target::getPlatformString(p) );

        if( auto Err = Entry.m_LogFile.Open( Path.To<xwchar>(), "wt" ); Err.isError() )
        {
            X_LOG_CHANNEL_ERROR( m_LogChannel, "Fail to create the log file [%s]", Path.To<xchar>().getPtr() );
            return { err::state::FAILURE, "Fail to create the log file" };
        }
    }
    */

    //
    // Create the log 
    //
    /*
    {
        xcore::cstring Path;
        Path = xcore::string::Fmt( "%s/Log.txt", m_LogsPath.data() );
        if( auto Err = m_LogFile.open( xcore::string::To<wchar_t>(Path), "wt" ); Err )
        {
            XLOG_CHANNEL_ERROR( m_LogChannel, "Fail to create the log file [%s]", Path.data() );
            return xerr_failure_s( "Fail to create the log file" );
        }
    }
    */

    try
    {
        //
        // Get the timer
        //
        {
            LogMessage(msg_type::INFO, "------------------------------------------------------------------");
            LogMessage(msg_type::INFO, " Start Compilation");
            LogMessage(msg_type::INFO, "------------------------------------------------------------------");
        }

        //
        // Do the actual compilation
        //
        if( auto Err = onCompile(); Err )
            return Err;

        //
        // Save the dependency file
        //
        if (m_Dependencies.hasDependencies())
        {
            //
            // Normalize asset dependencies
            //
            if ( false == m_Dependencies.m_Assets.empty() )
            {
                for (auto& S : m_Dependencies.m_Assets)
                {
                    std::ranges::transform(S, S.begin(), [](auto c) {return std::tolower(c); });
                    std::ranges::transform(S, S.begin(), [](auto c) {return c == '/' ? '\\' : c; });
                }

                // Sort the vector to bring duplicates together
                std::sort(m_Dependencies.m_Assets.begin(), m_Dependencies.m_Assets.end());

                // Remove consecutive duplicates
                m_Dependencies.m_Assets.erase(std::unique(m_Dependencies.m_Assets.begin(), m_Dependencies.m_Assets.end()), m_Dependencies.m_Assets.end());
            }

            for (auto& S : m_Dependencies.m_VirtualAssets)
                std::ranges::transform(S, S.begin(), [](auto c){ return std::tolower(c); });


            xproperty::settings::context C{};
            if (auto Err = m_Dependencies.Serialize( false, std::format( L"{}/dependencies.txt", m_ResourceLogPath), C ); Err)
                return Err;
        }

        //
        // Get the timer
        //
        {
            LogMessage(msg_type::INFO, "------------------------------------------------------------------");
            LogMessage(msg_type::INFO, std::format("Compilation Time: {:.3f}s", std::chrono::duration<float>(std::chrono::steady_clock::now() - m_Timmer).count()) );
            LogMessage(msg_type::INFO, "------------------------------------------------------------------");
        }
    }
    catch (std::runtime_error RunTimeError)
    {
        LogMessage(msg_type::ERROR, std::format("{}", RunTimeError.what()));

        return xerr::create_f<state, "FAILED: exception thrown exiting...">();
    }

    printf("[COMPILATION_SUCCESS]\n");
    return {};
}

} //namespace xasset_pipeline::compiler