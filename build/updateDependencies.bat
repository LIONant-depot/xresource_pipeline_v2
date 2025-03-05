@echo OFF
setlocal enabledelayedexpansion
set XRSCPIPELINE_PATH=%cd%

rem --------------------------------------------------------------------------------------------------------
rem Set the color of the terminal to blue with yellow text
rem --------------------------------------------------------------------------------------------------------
COLOR 8E
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore Cyan Welcome I am your XRESOURCE PIPELINE V2 dependency updater bot, let me get to work...
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
echo.

:DOWNLOAD_DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XRESOURCE PIPELINE - DOWNLOADING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
echo.

rem ------------------------------------------------------------
rem XCORE
rem ------------------------------------------------------------
:XCORE
rmdir "../dependencies/xcore" /S /Q
git clone https://gitlab.com/LIONant/xcore.git "../dependencies/xcore"
if %ERRORLEVEL% GEQ 1 goto :ERROR
cd ../dependencies/xcore/builds
if %ERRORLEVEL% GEQ 1 goto :ERROR
call UpdateDependencies.bat "return"
if %ERRORLEVEL% GEQ 1 goto :ERROR
cd /d %XRSCPIPELINE_PATH%
if %ERRORLEVEL% GEQ 1 goto :ERROR

rem ------------------------------------------------------------
rem XPROPERTY
rem ------------------------------------------------------------
:XPROPERTY
rmdir "../dependencies/xproperty" /S /Q
git clone https://github.com/LIONant-depot/xproperty.git "../dependencies/xproperty"
if %ERRORLEVEL% GEQ 1 goto :ERROR

rem ------------------------------------------------------------
rem XRESOURCE
rem ------------------------------------------------------------
:XPROPERTY
rmdir "../dependencies/xresource" /S /Q
git clone https://github.com/LIONant-depot/xresource.git "../dependencies/xresource"
if %ERRORLEVEL% GEQ 1 goto :ERROR

rem ------------------------------------------------------------
rem DONE
rem ------------------------------------------------------------
:DONE
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XRESOURCE PIPELINE V2 - DONE!!
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
goto :PAUSE

:ERROR
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------
powershell write-host -fore Red XRESOURCE PIPELINE V2 - ERROR!!
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------

:PAUSE
rem if no one give us any parameters then we will pause it at the end, else we are assuming that another batch file called us
if %1.==. pause