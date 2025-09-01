#ifndef XRESOURCE_PIPELINE_V2_H
#define XRESOURCE_PIPELINE_V2_H
#pragma once

#include <vector>
#include <string>
#include <array>

//------------------------------------------------------------
// The expected directory structure
//------------------------------------------------------------
//                                              //
//  + Database Name.lion_rcdbase                // All resources that are compiled are here. Deleting this directory should be ok since everything is generated.
//      + (full-guid)                           // Particular Instance of the library or project ((guid))
//          + Browser.dbase                     // Log and other usefull information about the compilation and be put here
//              + Resource_Type_Guid            // Which resource type are we dealing with
//                  + Resource_Instance_Guid    // folder of the particular resource
//          + Windows.platform                  // Resources that are compiled for the windows platform
//              + Data                          // Inside here you will find the final resources with their full guids in a flat list
//          + Generated.dbase                   // Temporary generated assets which are then compiled into the platform specific directories
//              + Resource_Type_Guid            // Different types of resources
//                  + Resource_Instance_Guid    // Instance of a resource
//  + Project Name.lion_project                 // Project can be located any where (This can go to source control)
//      + Resources                             // Resources for this project/library
//          + "Resource Type"                   // Different resource types such (textures, geom, editor pluings, etc..)
//          + "Editor.plugins"                  // Example of a resource type
//              + Editor                        // Inside this resource type may can be organize however it wants
//              + Plugin-guid                   // ...
//          + "Resource.pipeline"               // Example of another resource
//              + "Compiler-guid"               // Every compiler guid
//                  + Debug                     // Debug compilers used for development and debugging issues
//                  + Release                   // Here are the standard compilers
//                  * DefaultSettings.txt       // The default settings for the compilers
//                  * DefaultIcon.dds           // Icon for the editor and such
//          + "Resource Tags Type"              //  
//              + "Instance of a tag guid"      //
//      + Scenes                                // Scenes
//      + Configs                               // Configuration files
//  + Version_23.lion_editor                    // Editor folder
//      + Resources                             // Resources for the editor
//          + "Editor.plugins"                  // Example of a resource type, Editor plugins
//          + "Resource.pipeline"               // Example of another resource type
//      + Configs                               // Configuration files
//      * Editor.exe                            // exe for the editor
//  + Assets (Artist controlled)                // Directory containing the assets can be named anything
//      + bla bla...                            //
//------------------------------------------------------------
// 
// Note that this system does not deal with the source assets. Those should be located in a different set of directories usually managed by artist 
// 
// A resource is fully identified using their full_guids. These full guids contains: (Asset_type_guid) + (Asset_Instance_Guid)
// So they are a total of 128bits.
// 
// Steps to build data base
// ------------------------
// "Path to the Library"/Config/ResourcePipeline.config
//      - Destination folder for this resource ("Database Name.qlion_rcdbase")
//      - Location of the resource pipeline ("Where are the compilers for each resource types")
//      - Guid of the library/project
// 
// "Path to the Library"/Resource/"Resource Pipeline type guid"/"Particular Asset type Guid"/DefaultSettings.config
//      - What should be the default compiler settings
//      - originally copied from some where .... may be
// 
// "Path to the library"/Resource/"Resource Type Guid"/"Resource Instance Guid"/Descriptor.config
//      - Custom options to compile this particular resource
//      - Location of assets
// 
// Loading the resource 
// -----------------------
// Loading the resources you will need:
// + List of all libraries (Guids and Paths)
// 
// A resource fill need to be search in each of the resource libraries until it finds it or fails
// 
//------------------------------------------------------------

namespace xresource_pipeline
{
    enum class state : std::uint8_t
    { OK            = 0
    , FAILURE       
    , DISPLAY_HELP
    };

    enum class platform : std::uint8_t
    { WINDOWS = 0
    , MAC     
    , IOS     
    , LINUX   
    , ANDROID 
    , COUNT
    };

    inline constexpr static std::array wplatform_v
    { L"WINDOWS"
    , L"MAC"
    , L"IOS"
    , L"LINUX"
    , L"ANDROID"
    };

    enum class msg_type : std::uint8_t
    { INFO
    , WARNING
    , ERROR
    };
}

#include "dependencies/xstrtool/source/xstrtool.h"
#include "dependencies/xproperty/source/examples/imgui/my_properties.h"
#ifndef XRESOURCE_PIPELINE_NO_COMPILER
    #include "dependencies/xproperty/source/examples/imgui/my_property_ui_null.h"
#endif
#include "dependencies/xproperty/source/examples/xcore_sprop_serializer/xcore_sprop_serializer.h"

#include "dependencies/xresource/source/xresource_guid.h"
#include "dependencies/xresource/source/xresource_guid_properties.h"


#include "xresource_pipeline_tag.h"
#include "xresource_pipeline_dependencies.h"
#include "xresource_pipeline_version.h"
#include "xresource_pipeline_descriptor_base.h"
#include "xresource_pipeline_info.h"
#include "xresource_pipeline_factory.h"
#include "xresource_pipeline_compiler_base.h"

// INLINES
#include "details/xresource_pipeline_factory_inline.h"

void displayProgressBar(const char* pTitle, float progress) noexcept;


#endif