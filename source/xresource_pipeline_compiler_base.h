#ifndef XRESOURCE_PIPELINE_V2_H
#error "You must include xresource_pipeline.h before you include this file" 
#endif
#pragma once

#include <source_location>

namespace xresource_pipeline::compiler
{
    //
    // Command line arguments
    // ---------------------
    // -OPTIMIZATION    O1                                  // Optional
    // -DEBUG           D1                                  // Optional
    // -TARGET          WINDOWS                             // Optional
    // -PROJECT         "C:\SomeFolder\MyProject.lion" 
    // -DESCRIPTOR      "Texture\00\faff00.desc"
    // -OUTPUT          "C:\SomeFolder\MyProject.lion\Temp\Resources\Build"
    // More info in [CommanLineParameters.md](Documentation\CommanLineParameters.md)
    class base
    {
    public:
        
       // using imported_file_fn = xcore::func< void( std::wstring_view FilePath ) >;

        enum class optimization_type
        { INVALID
        , O0                // Compiles the asset as fast as possible no real optimization
        , O1                // (Default) Build with optimizations
        , Oz                // Take all the time you like to optimize this resource
        };

        enum class debug_type
        { INVALID
        , D0                // (Default) Compiles with some debug
        , D1                // Compiles with extra debug information
        , Dz                // Runs the debug version of the compiler and such... max debug level
        };

        struct platform
        {
            bool                            m_bValid            { false };                  // If we need to build for this platform
            xresource_pipeline::platform    m_Platform          { xresource_pipeline::platform::WINDOWS };      // Platform that we need to compile for
            std::wstring                    m_DataPath          {};                         // This is where the compiler need to drop all the compiled data
        };

        struct project_paths
        {
            std::wstring          m_Project               = {}; // Project or Library that contains the resource (note that this is the root)
            std::wstring          m_Descriptors           = {};
            std::wstring          m_Config                = {};
            std::wstring          m_Assets                = {};
            std::wstring          m_Cache                 = {};
            std::wstring          m_CacheTemp             = {};
            std::wstring          m_CachedDescriptors     = {};
            std::wstring          m_Resources             = {};
            std::wstring          m_ResourcesPlatforms    = {};
            std::wstring          m_ResourcesLogs         = {};
            std::wstring          m_Output                = {};
        };

    public:
        
        virtual                                    ~base                        ( void )                                                        noexcept;
                                                    base                        ( void )                                                        noexcept;

                            xerr                    Compile                     ( void )                                                        noexcept;
                            xerr                    Parse                       ( int argc, const char* argv[] )                                noexcept;
                            void                    displayProgressBar          (const char* pTitle, float progress)                    const   noexcept;
                            const std::wstring_view getDestinationPath          ( xresource_pipeline::platform p )                      const   noexcept;
                            xerr                    CreatePath                  ( std::wstring_view Path )                              const   noexcept;

    protected:
        
        virtual             xerr                    onCompile                   ( void )                                                                 = 0;
                            void                    LogMessage                  (xresource_pipeline::msg_type Type, std::string&& String,    const std::source_location loc = std::source_location::current()) const noexcept;
                            void                    LogMessage                  (xresource_pipeline::msg_type Type, std::wstring&& String,   const std::source_location loc = std::source_location::current()) const noexcept;
                            void                    LogMessage                  (xresource_pipeline::msg_type Type, std::string_view String, const std::source_location Loc = std::source_location::current()) const noexcept;
                            void                    LogMessage                  (xresource_pipeline::msg_type Type, const char* pString,     const std::source_location Loc = std::source_location::current()) const noexcept;

    private:
                            xerr                    InternalParse               ( const int argc, const char *argv[] );
                            xerr                    setupPaths                  ( void )                                                        noexcept;

    protected:

        using platform_list = std::array<platform, static_cast<int>(xresource_pipeline::platform::COUNT)>;

        debug_type                                              m_DebugType                 { debug_type::D0 };
        optimization_type                                       m_OptimizationType          { optimization_type::O1 };
        std::chrono::steady_clock::time_point                   m_Timmer                    {};

        project_paths                                           m_ProjectPaths              {}; // Standard path for the pipeline

        std::wstring                                            m_InputSrcDescriptorPath    {}; // Path to of the descriptor that we have been asked to compile
        std::wstring                                            m_ResourceType              {};
        std::wstring                                            m_ResourcePartialPath       {};
        std::wstring                                            m_ResourceLogPath           {};
        FILE*                                                   m_pLogFile                  {};

        platform_list                                           m_Target                    {};
        dependencies                                            m_Dependencies              {};

    protected:

        friend void LogFunction(msg_type Type, std::string_view String, std::uint32_t Line, std::string_view file) noexcept;
        friend void xerr_ErrorHandler(const char* StateStr, std::uint8_t state, std::string_view msg, std::uint32_t line, std::string_view file);
    };
}
