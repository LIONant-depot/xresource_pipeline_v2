# *Commandline Parameters*

These command line parameters are meant to be used for all resource compilers. 
They are designed to be simple and easy to use. 

## `-OPTIMIZATION` argument
This parameter is used to specify how much work the compiler should do to build the resource.
This work may affect the final quality of the resource.

Argument requirements:
* This argument is optional. If not present assumes basic optimizations.

Arguments:
* `O0` - Compiles the resource as fast as possible no real optimization\quality
* `O1` - Build with basic optimizations (Default)
* `Oz` - Take all the time you like to optimize this resource

Example:
* -OPTIMIZATION Oz

## `-DEBUG` argument
How much debug information should the compiler add to the resource for cases where debugging is needed and avariable.

Argument requirements:
* This argument is optional. If not present assumes no debug information.

Arguments:
* `D0` - No debug (Default)
* `D1` - Debug information
* `Dz` - Max debug level, (Can make the resource file large and slow)

Example:
* -DEBUG D0

## `-PLATFORM` many_arguments
Which plaforms should the compiler compile for. 

Argument requirements:
* This argument is optional. if not specified it is assumed to be windows.

Arguments:
* `WINDOWS` - Will compile for windows (Default)
* `ANDROID` - Will compile for android

Examples:
* -PLATFORM WINDOWS ANDROID
* -PLATFORM WINDOWS

## `-PROJECT` argument
Full path to the project folder including the project name.
We recommend that your project name have some extension such as .lion. 
This will help other tools to recognize the project file.

### **WARNING:** 
It is recommended that the path to the project should have not spaces.
Which means no folders with spaces not even in the project name.
Because ther tools may not be able to handle spaces in the path property. 
We recoomend using mix cases to solve this problem.

Argument requirements:
* This argument is required.
* Single string, required and should be done with quotes
* Full patth to the project folder including the project name.

Examples:
* -PROJECT "C:\MyProject.lion"
* -PROJECT "C:\SomeFolder\MyProject.lion"

## `-DESCRIPTOR` argument
The argument is the path to the descritor firectory.
Note that the path starts at the project root.
The reasons for this is because from the 
point of view of the compiler it does not know if this is 
a resource or a virtual resource or something else.

Argument requirements:
* This argument is required.
* Single string, required and should be done with quotes
* Relative path to the project folder.

Examples:
* -DESCRIPTOR "Descriptors\Texture\00\faff00.desc"
* -DESCRIPTOR "Descriptors\Mesh\faff10.desc"

## `-OUTPUT` argument
Full path of the output directory, please note that it does not specify the platform.
Why the full path? Because this way keeps it more future proof.
For instance it could allow the compiler to output resources to 
a network drive.

Argument requirements:
* This argument is required.
* Single string, required and should be done with quotes
* Full path to the output directory including the drive letter.

Example:
* -OUTPUT "C:\MyProject.lion\Temp\Resources\Build"


---