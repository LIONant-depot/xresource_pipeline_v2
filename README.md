# xresource_pipeline V2.0.0

The resource pipeline is a common set of source file and concepts that define a workflow for resources.
These resource are used as the principal data for games and other tools.
The source files are design for command-line tool that can be used to generate resources.

# Key Concepts
* [Terminology](documentation/Terminology.md)
* [Folder organization](documentation/FolderOrganization.md)
* [Command line parameters](documentation/CommadLineParameters.md)

# Key features
* License MIT
* C++ 20
* Simple yet flexible structure
* Clarity on concepts
* Mutiuser and source control awareness 
* Management of Directory structure for resources
* Comman-line parsing for resource compilers

# Prerequisites
* [CMake](https://cmake.org/download/) (Must run from command line)

# Installation

Run the command below to install the resource pipeline.
(You can double click on the batch file as well)
```
build/updateDependencies.bat
```

You can then include in your projected:
```
src/xresource_pipeline.h
src/xresource_pipeline.cpp
```

# Dependencies 
* [xcore](https://gitlab.com/LIONant/xcore)


---
