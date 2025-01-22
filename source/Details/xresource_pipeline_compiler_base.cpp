
#include "xresource_pipeline.h"

namespace xresource_pipeline::compiler {

base* g_pBase = nullptr;

//---------------------------------------------------------------------------------------

void LogFunction( const xcore::log::channel& Channel, xcore::log::msg_type Type, const char* pString, int Line, const char* file ) noexcept
{
    if (pString == nullptr) pString = "Unkown";

    //
    // Print to the standard output
    //
    {
        static bool bPendingNewLine = false;
        if (pString[0] == '\r')
        {
            printf
            ("\r[%s] %s"
            , xcore::log::msg_type::L_INFO == Type ? "Info" : xcore::log::msg_type::L_WARNING == Type ? "Warning" : "Error"
            , &pString[1]
            );
            bPendingNewLine = true;
        }
        else
        {
            // In case of surprised warning or errors while doing pertentages we need to make sure we print a new line
            if (bPendingNewLine) printf("\n");
            bPendingNewLine = false;

            printf
            ("[%s] %s\n"
            , xcore::log::msg_type::L_INFO == Type ? "Info" : xcore::log::msg_type::L_WARNING == Type ? "Warning" : "Error"
            , pString
            );
        }

        // Make sure we always print the message
        std::fflush(stdout);
    }

    //
    // Save to the log file
    //
    if( g_pBase->m_LogFile.isOpen() ) 
    {
        if( auto Err = g_pBase->m_LogFile.Printf
            ( "%s(%d) [%s] %s\n"
            , file ? file : "Unkown file"
            , Line
            , xcore::log::msg_type::L_INFO == Type ? "Info"
            : xcore::log::msg_type::L_WARNING == Type ? "Warning"
            : "Error"
            , pString[0] == '\r' ? &pString[1] : pString
            ); Err )
        {
            printf( "Fail to save data in log\n" );
            Err.clear();
        }

        // Make sure things are written in time...
        g_pBase->m_LogFile.Flush();
    }
}

//--------------------------------------------------------------------------

base::base( void ) noexcept
{
    // Install the log function
    xcore::get().m_MainLogger.m_pOutput = LogFunction;
    g_pBase = this;
}

//------------------------------------------------------------------------------
// https://superuser.com/questions/1362080/which-characters-are-invalid-for-an-ms-dos-filename
// Try to create a very compact resource file name
//------------------------------------------------------------------------------
template< typename T_CHAR, int extra_v=0, typename T > constexpr
xcore::string::ref<T_CHAR> CompactName( T aVal ) noexcept
{
    static_assert(std::is_integral_v<T>);
    using t = xcore::types::to_uint_t<T>;
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

//--------------------------------------------------------------------------

xcore::err base::setupPaths( void ) noexcept
{
    //
    // Get resource partial path should be something like this: "ResourceType/0F/23/321230F23"
    //
    {
        int iStartString;
        int iEndString;
        if( xcore::string::FindStrI(m_InputSrcDescriptorPath, "Descriptors") == 0 )
        {
            iStartString = xcore::string::FindStr(m_InputSrcDescriptorPath, "/");
        }
        else
        {
            if (xcore::string::FindStrI(m_InputSrcDescriptorPath, "Cache") != 0) return xerr_failure_s("Expecting a descriptor path that starts with a 'C' for Cache or 'D' for Descriptor");
            iStartString = xcore::string::FindStr(m_InputSrcDescriptorPath, "/");
            if (iStartString >= 0) iStartString = xcore::string::FindStr(&m_InputSrcDescriptorPath[iStartString + 1], "/");
        }
        iEndString = xcore::string::findLastInstance(m_InputSrcDescriptorPath, '.');
        if (iEndString < 0)  return xerr_failure_s("Expecting a descriptor path that has an extension '.'");
        if (iStartString < 0) return xerr_failure_s("The descriptor did not included a valid path (1)");

        // Now that should have something that looks like this: "ResourceType/0F/23/321230F23"
        m_ResourcePartialPath.copy_n( &m_InputSrcDescriptorPath[iStartString+1], iEndString - iStartString - 1);
    }

    //
    // Get the resource type string
    //
    {
        int iLastCharOfTheType = xcore::string::FindStr(m_ResourcePartialPath, "/");
        if (iLastCharOfTheType < 0) return xerr_failure_s("The descriptor did not included a valid path (2)");
        m_ResourceType.copy_n(m_ResourcePartialPath, iLastCharOfTheType);
    }

    //
    // Set all the core directories
    //
    m_ProjectPaths.m_Descriptors = xcore::string::Fmt("%s/Descriptors", m_ProjectPaths.m_Project.data());
    m_ProjectPaths.m_Config = xcore::string::Fmt("%s/Config", m_ProjectPaths.m_Project.data());
    m_ProjectPaths.m_Assets = xcore::string::Fmt("%s/Assets", m_ProjectPaths.m_Project.data());
    m_ProjectPaths.m_Cache = xcore::string::Fmt("%s/Cache", m_ProjectPaths.m_Project.data());
    m_ProjectPaths.m_CacheTemp = xcore::string::Fmt("%s/Temp", m_ProjectPaths.m_Cache.data());
    m_ProjectPaths.m_CachedDescriptors = xcore::string::Fmt("%s/Descriptors", m_ProjectPaths.m_Cache.data());
    m_ProjectPaths.m_Resources = xcore::string::Fmt("%s/Resources", m_ProjectPaths.m_Cache.data());
    m_ProjectPaths.m_ResourcesPlatforms = xcore::string::Fmt("%s/Platforms", m_ProjectPaths.m_Resources.data());
    m_ProjectPaths.m_ResourcesLogs = xcore::string::Fmt("%s/Logs", m_ProjectPaths.m_Resources.data());

    //
    // Set up the path required for this complilation
    //
    m_ResourceLogPath = xcore::string::Fmt("%s/%s.log"
        , m_ProjectPaths.m_ResourcesLogs.data()
        , m_ResourcePartialPath.data()
    );

    // Make sure the log path is ready
    if (auto Err = base::CreatePath(m_ResourceLogPath); Err)
        return Err;

    // Open the log file
    if (auto Err = m_LogFile.open(xcore::string::To<wchar_t>(xcore::string::Fmt("%s/Log.txt", m_ResourceLogPath.data())), "wt"); Err)
        return xerr_failure_s("Fail to create the log file");

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
          xcore::cstring EverythingExceptTheResourceGUID;

          int iLast = xcore::string::findLastInstance(m_ResourcePartialPath, '/');
          if (iLast < 0) return xerr_failure_s("The descriptor did not included a valid path (3)");

          EverythingExceptTheResourceGUID.copy_n(m_ResourcePartialPath, iLast);

        for (auto& E : m_Target)
        {
            if (E.m_bValid)
            {
                // This should end up like... "FullPath/Project.lion_project/Cache/Platforms/Windows/ResourceType/0F/23"
                E.m_DataPath = xcore::string::Fmt("%s/%s/%s"
                    , m_ProjectPaths.m_ResourcesPlatforms.data()
                    , xcore::target::getPlatformString(E.m_Platform)
                    , EverythingExceptTheResourceGUID.data()
                    );

                // Make sure is ready for rock and roll
                if (auto Err = base::CreatePath(E.m_DataPath); Err)
                    return Err;

                // OK then now everything including the final file name (no extension)
                // This should end up like... "FullPath/Project.lion_project/Cache/Platforms/Windows/ResourceType/0F/23/4fdsdf230F"
                E.m_DataPath = xcore::string::Fmt("%s/%s/%s"
                    , m_ProjectPaths.m_ResourcesPlatforms.data()
                    , xcore::target::getPlatformString(E.m_Platform)
                    , m_ResourcePartialPath.data()
                );

            }
        }
    }

    return {};
}

//--------------------------------------------------------------------------

xcore::err base::InternalParse( const int argc, const char *argv[] )
{
    xcore::cmdline::parser CmdLineParser;
    
    //
    // Create the switches and their rules
    //
    {
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("OPTIMIZATION"),  1,  1, 0, 1, false );
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("DEBUG"),         1,  1, 0, 1, false );
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("TARGET"),        1, -1, 0, 1, false );
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("DESCRIPTOR"),    1,  1, 1, 1, false );
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("PROJECT"),       1,  1, 1, 1, false );
        CmdLineParser.AddCmdSwitch( xcore::string::const_crc("OUTPUT"),        1,  1, 1, 1, false );
    }
    
    //
    // Start parsing the arguments
    //
    if( auto Err =  CmdLineParser.Parse( argc, argv ); Err )
        return Err;
    
    //
    // Deal with the help message
    //
    if( argc < 2 || CmdLineParser.DoesUserNeedsHelp() )
    {
        std::printf( "\n"
                  "-------------------------------------------------------------\n"
                  "LION - Compiler system.                                      \n"
                  "%s [ %s - %s ]                                               \n"
                  "Switches: (Make sure they are in order)                      \n"
                  "     -OPTIMIZATION  O0 - Compile as fast as possible         \n"
                  "                    Q1 - Compile with optimizations          \n"
                  "                    Qz - Maximum performance for asset       \n"
                  "     -DEBUG         D0 - Basic debug information             \n"
                  "                    D1 - Extra debug information             \n"
                  "                    Dz - Maximum debug information           \n"
                  "     -TARGET     <WINDOWS OSX IOS ANDROID>                   \n"
                  "     -PROJECT    <NetworkFriendlyPathToProject.lion_project> \n"
                  "     -OUTPUT     <NetworkFriendlyPathToProject.lion_project\\Output> \n"
                  "     -DESCRIPTOR <RelativeToProject.desc>                    \n"
                  "-------------------------------------------------------------\n",
                 argv[0],
                 __TIME__, __DATE__ );
        
        return xerr_code_s( error::DISPLAY_HELP, "User requested help" );
    }
    
    //
    // Go through the parameters of the command line
    //
    xcore::cstring InputName;
    bool           UseDefaultTaget = true;
    for( int i=0; i< CmdLineParser.getCommandCount(); i++ )
    {
        const auto  Cmd     = CmdLineParser.getCommand(i);
        const auto  CmdCRC  = Cmd.getCRC();
        if( CmdCRC == xcore::types::value<xcore::crc<32>::FromString( "TARGET" )> )
        {
            const auto Count = Cmd.getArgumentCount(); 
            if (Count) UseDefaultTaget = false;
            for( int i=0; i<Count; i++ )
            {
                const char* pString = Cmd.getArgument(i);
                bool        bFound  = false;
                
                // Go through all the platforms and pick up the right one
                constexpr auto TargetCount = xcore::target::getPlatformCount(); 
                for( int ip = 0; ip < TargetCount; ip++ )
                {
                    const auto p = xcore::types::static_cast_safe<xcore::target::platform>( ip );

                    if( xcore::string::CompareI( pString, xcore::target::getPlatformString(p) ) == 0 )
                    {
                        platform& NewTarget   = m_Target[ip];
                        
                        if( NewTarget.m_bValid )
                            return xerr_failure_s("The same platform was enter multiple times");
                        
                        NewTarget.m_Platform  = p;
                        NewTarget.m_bValid    = true;
                        
                        bFound = true;
                        break;
                    }
                }
                
               if( bFound == false )
                   return xerr_failure_s("Platform not supported");
            }
        }
        else if( CmdCRC == xcore::types::value<xcore::crc<32>::FromString( "PROJECT" )> )
        {
            if (   -1 == xcore::string::FindStr(Cmd.getArgument(0), ".lion_project")
                && -1 == xcore::string::FindStr(Cmd.getArgument(0), ".lion_library") )
                return xerr_failure_s("I got a path in the PROJECT switch that is not a .lion_project or .lion_library path");

            xcore::string::Copy(m_ProjectPaths.m_Project, Cmd.getArgument( 0 ) );
            xcore::string::CleanPath(m_ProjectPaths.m_Project);
        }
        else if (CmdCRC == xcore::types::value<xcore::crc<32>::FromString("OUTPUT")>)
        {
            xcore::string::Copy(m_ProjectPaths.m_Output, Cmd.getArgument(0));
            xcore::string::CleanPath(m_ProjectPaths.m_Output);
        }
        else if (CmdCRC == xcore::types::value<xcore::crc<32>::FromString("DESCRIPTOR")>)
        {
            xcore::string::Copy(m_InputSrcDescriptorPath, Cmd.getArgument(0));

            if ( -1 == xcore::string::FindStrI(m_InputSrcDescriptorPath, ".desc" ) )
                return xerr_failure_s("You must specify the descriptor path which includes the extension");

            xcore::string::CleanPath(m_InputSrcDescriptorPath);
        }
        else if( CmdCRC == xcore::types::value<xcore::crc<32>::FromString( "OPTIMIZATION" )> )
        {
            xcore::cstring BuildType;

            xcore::string::Copy( BuildType, Cmd.getArgument( 0 ) );
            
            if( BuildType == xcore::string::constant("O0") )
            {
                m_OptimizationType = optimization_type::O0;
            }
            else if( BuildType == xcore::string::constant("O1") )
            {
                m_OptimizationType = optimization_type::O1;
            }
            else if( BuildType == xcore::string::constant("Oz") )
            {
                m_OptimizationType = optimization_type::Oz;
            }
            else
            {
                return xerr_failure_s("Optimization Type not supported");
            }
        }
        else if( auto crc = xcore::types::value<xcore::crc<32>::FromString("DEBUG")>; CmdCRC == crc )
        {
            xcore::cstring BuildType;

            xcore::string::Copy( BuildType, Cmd.getArgument( 0 ) );

            if( BuildType == xcore::string::constant("D0") )
            {
                m_DebugType = debug_type::D0;
            }
            else if( BuildType == xcore::string::constant("D1") )
            {
                m_DebugType = debug_type::D1;
            }
            else if( BuildType == xcore::string::constant("Dz") )
            {
                m_DebugType = debug_type::Dz;
            }
            else
            {
                return xerr_failure_s("Debug Type not supported");
            }
        }
        else
        {
            // We must have forgotten a switch because we should not be able to reach this point
            return xerr_failure_s("Compiler found unknown arguments");
        }
    }

    if (UseDefaultTaget)
    {
        m_Target[static_cast<int>(xcore::target::platform::WINDOWS)].m_bValid = true;
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
        XLOG_CHANNEL_INFO(m_LogChannel, "Compiler command line options");
        XLOG_CHANNEL_INFO(m_LogChannel, "PROJECT: %s", m_ProjectPaths.m_Project.c_str());

        {
            std::string TargetString;
            for (auto& E : m_Target)
            {
                if (E.m_bValid)
                {
                    TargetString += xcore::target::getPlatformString(E.m_Platform);
                    TargetString += " ";
                }
            }
            XLOG_CHANNEL_INFO(m_LogChannel, "TARGETS: %s", TargetString.c_str() );
        }

        XLOG_CHANNEL_INFO(m_LogChannel, "OUTPUT: %s", m_ProjectPaths.m_Output.c_str());
        XLOG_CHANNEL_INFO(m_LogChannel, "DESCRIPTOR: %s", m_InputSrcDescriptorPath.c_str());
        XLOG_CHANNEL_INFO(m_LogChannel, "OPTIMIZATION: %s", m_OptimizationType == optimization_type::O0 ? "O0" : m_OptimizationType == optimization_type::O1 ? "O1" : "Oz");
        XLOG_CHANNEL_INFO(m_LogChannel, "DEBUG: %s", m_DebugType == debug_type::D0 ? "D0" : m_DebugType == debug_type::D1 ? "D1" : "Dz");
    }

    return {};
}

//--------------------------------------------------------------------------

xcore::cstring base::getDestinationPath( xcore::target::platform p ) const noexcept
{
    const int Index = static_cast<int>(p);
    xassert( m_Target[Index].m_bValid );
    xassert( m_Target[Index].m_Platform == p );
    return m_Target[Index].m_DataPath;
}

//--------------------------------------------------------------------------

xcore::err base::Parse( int argc, const char *argv[] ) noexcept
{
    try
    {
        if( auto Err = InternalParse( argc, argv ); Err )
        {
            if( Err.getCode().getState<xresource_pipeline::error>() != xresource_pipeline::error::DISPLAY_HELP )
            {
                // TODO: We should open the log file before this???
                XLOG_CHANNEL_ERROR(m_LogChannel, "Parsing error (%s)", Err.getCode().m_pString);
            }
            else
            {
                // If we are displaying help we just return...
                Err.clear();
            }
            
            return Err;
        }
    }
    catch (std::runtime_error RunTimeError)
    {
        XLOG_CHANNEL_ERROR(m_LogChannel, "%s", RunTimeError.what());
        return xerr_failure_s("FAILED: exception thrown exiting...");
    }

    return {};
}

//--------------------------------------------------------------------------

xcore::err base::CreatePath( const xcore::cstring& Path ) const noexcept
{
    std::error_code         ec;
    std::filesystem::path   path{ xcore::string::To<wchar_t>(Path).data() };

    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        XLOG_CHANNEL_ERROR(m_LogChannel, "Fail to create a directory [%s] with error [%s]", Path.data(), ec.message().c_str());
        return xerr_failure_s("Fail to create a directory");
    }

    return {};
}

//--------------------------------------------------------------------------

void base::displayProgressBar(const char* pTitle, float progress) const noexcept
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    constexpr auto total_chars_v = 40;
    constexpr auto fill_progress = "========================================";
    constexpr auto empty_progress = "                                        ";
    constexpr auto bar_width = 35;
    const auto     pos = static_cast<int>(bar_width * progress);
    const auto     filled = total_chars_v - pos;
    const auto     empty = total_chars_v - (bar_width - pos);

    XLOG_CHANNEL_INFO(m_LogChannel, "\r%s: [%s>%s] %3d%%", pTitle, &fill_progress[filled], &empty_progress[empty], static_cast<int>(progress * 100.0));
}


//--------------------------------------------------------------------------

xcore::err base::Compile( void ) noexcept
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
            XLOG_CHANNEL_INFO(m_LogChannel, "------------------------------------------------------------------");
            XLOG_CHANNEL_INFO(m_LogChannel, " Start Compilation" );
            XLOG_CHANNEL_INFO(m_LogChannel, "------------------------------------------------------------------" );
        }

        //
        // Do the actual compilation
        //
        if( auto Err = onCompile(); Err )
            return Err;

        //
        // Save the dependency file
        //         
       // if( auto Err = m_Dependencies.Serialize( m_Dependencies, xcore::string::Fmt( "%s/%s", m_BrowserPath.data(), xresource_pipeline::resource_dependencies_name_v.data()).data(), false); Err )
       //     return Err;

        //
        // Get the timer
        //
        {
            XLOG_CHANNEL_INFO(m_LogChannel, "------------------------------------------------------------------");
            XLOG_CHANNEL_INFO(m_LogChannel, " Compilation Time: %.3fs", std::chrono::duration<float>(std::chrono::steady_clock::now() - m_Timmer).count());
            XLOG_CHANNEL_INFO(m_LogChannel, "------------------------------------------------------------------");
        }
    }
    catch (std::runtime_error RunTimeError)
    {
        XLOG_CHANNEL_ERROR(m_LogChannel, "%s", RunTimeError.what() );
        return xerr_failure_s("FAILED: exception thrown exiting...");
    }

    printf("[COMPILATION_SUCCESS]\n");
    return {};
}

} //namespace xasset_pipeline::compiler