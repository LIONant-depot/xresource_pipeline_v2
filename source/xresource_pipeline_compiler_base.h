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
        
        using imported_file_fn = xcore::func< void( const xcore::cstring& FilePath ) >;

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
            bool                    m_bValid            { false };                              // If we need to build for this platform
            xcore::target::platform m_Platform          { xcore::target::platform::WINDOWS };   // Platform that we need to compile for
            xcore::cstring          m_DataPath          {};                                     // This is where the compiler need to drop all the compiled data
        };

        struct project_paths
        {
            xcore::cstring                                          m_Project{}; // Project or Library that contains the resource (note that this is the root)
            xcore::cstring                                          m_Descriptors{};
            xcore::cstring                                          m_Config{};
            xcore::cstring                                          m_Assets{};
            xcore::cstring                                          m_Cache{};
            xcore::cstring                                          m_CacheTemp{};
            xcore::cstring                                          m_CachedDescriptors{};
            xcore::cstring                                          m_Resources{};
            xcore::cstring                                          m_ResourcesPlatforms{};
            xcore::cstring                                          m_ResourcesLogs{};
            xcore::cstring                                          m_Output{};
        };

    public:
        
        virtual                                        ~base                        ( void )                                                        noexcept = default;
                                                        base                        ( void )                                                        noexcept;

                            xcore::err                  Compile                     ( void )                                                        noexcept;
                            xcore::err                  Parse                       ( int argc, const char* argv[] )                                noexcept;
                            void                        displayProgressBar          (const char* pTitle, float progress)                    const   noexcept;
                            xcore::cstring              getDestinationPath          ( xcore::target::platform p )                           const   noexcept;
                            xcore::err                  CreatePath                  ( const xcore::cstring& Path )                          const   noexcept;

    protected:
        
        virtual             xcore::guid::rcfull<>       getResourcePipelineFullGuid ( void )                                                const   noexcept = 0;
        virtual             xcore::err                  onCompile                   ( void )                                                                 = 0;

    private:
                            xcore::err                  InternalParse               ( const int argc, const char *argv[] );
                            xcore::err                  setupPaths                  ( void )                                                        noexcept;

    protected:
        
        debug_type                                              m_DebugType                 { debug_type::D0 };
        optimization_type                                       m_OptimizationType          { optimization_type::O1 };
        std::chrono::steady_clock::time_point                   m_Timmer                    {};


        std::uint64_t                                           m_RscGuid                   {}; // GUID of the resource

        project_paths                                           m_ProjectPaths              {};

        xcore::cstring                                          m_InputSrcDescriptorPath    {}; // Path to of the descriptor that we have been asked to compile
        xcore::cstring                                          m_ResourceType              {};
        xcore::cstring                                          m_ResourcePartialPath       {};
        xcore::cstring                                          m_ResourceLogPath           {};
        xcore::file::stream                                     m_LogFile                   {};

        std::array<platform, xcore::target::getPlatformCount()> m_Target                    {};

        xcore::log::channel                                     m_LogChannel                { "COMPILER" };     // Use this to emmit your errors/warnnings or information...
        //config::info                                            m_ConfigInfo                {};
//        int                                                     m_ConfigInfoIndex           {-1};

        dependencies                                            m_Dependencies              {};

    protected:

        friend void LogFunction(const xcore::log::channel& Channel, xcore::log::msg_type Type, const char* String, int Line, const char* file) noexcept;
    };
}
