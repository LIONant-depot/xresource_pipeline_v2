include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Fetch dependencies
FetchAndPopulate("https://github.com/LIONant-depot/xtextfile.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xcmdline.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xresource.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xproperty.git" "main")

DefineInterfaceComponent(xresource_pipeline_v2 "dependencies/xcore"
  "source/xresource_pipeline_version.h"
  "source/xresource_pipeline_tag.h"
  "source/xresource_pipeline_info.h"
  "source/xresource_pipeline_factory.h"
  "source/xresource_pipeline_descriptor_base.h"
  "source/xresource_pipeline_dependencies.h"
  "source/xresource_pipeline_config.h"
  "source/xresource_pipeline_compiler_base.h"
  "source/xresource_pipeline.h"
  "source/xresource_pipeline.cpp"
)