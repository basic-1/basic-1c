rem usage: win_7zip_zip_distr.bat <project_name> <platform> <compiler> [<is_next_revision>]

rem configurable path to 7-Zip archiver
set path_to_7z="C:\Program Files\7-Zip\7z.exe"

if not exist %path_to_7z% (
  echo 7z archiver not found
  goto :eof
)

setlocal EnableDelayedExpansion

if "%~1"=="" goto _invargs
if "%~2"=="" goto _invargs
if "%~3"=="" goto _invargs
goto _argsok

:_invargs
echo invalid arguments
goto :eof

:_argsok
set project_name=%~1
set platform=%~2
set compiler=%~3

if exist "..\..\env\win_%platform%_%compiler%_env.bat" call "..\..\env\win_%platform%_%compiler%_env.bat"

if not exist ..\..\common\source\gitrev.h (
  echo gitrev.h not found
) else (
  set /p build_num=<..\..\common\source\gitrev.h
  set build_num=!build_num:#define=!
  set build_num=!build_num:B1_GIT_REVISION=!
  set build_num=!build_num:"=!
  set build_num=!build_num: =!

rem do not increase git revision number read from gitrev.h
rem  if "%~4"=="next" (set /a build_num+=1)
)

rem get interpreter version
if not exist ..\..\common\source\version.h (
  echo version.h not found
  goto :eof
)
set /p version=<..\..\common\source\version.h
set version=%version:#define=%
set version=%version:B1_CMP_VERSION=%
set version=%version:"=%
set version=%version: =%

rem archive file name and path
set out_file_path=..\..\distr\%project_name%_win_%platform%_%compiler%_%version%

rem delete temp. directories
rmdir /s /q bin
rmdir /s /q docs

rem create docs temp. directory and copy document files there
mkdir docs
mkdir docs\b1c
copy ..\..\b1c\docs\*.* docs\b1c
mkdir docs\b1c\images
copy ..\..\b1c\docs\images\*.* docs\b1c\images
mkdir docs\c1stm8
copy ..\..\c1stm8\docs\*.* docs\c1stm8
mkdir docs\a1stm8
copy ..\..\a1stm8\docs\*.* docs\a1stm8

rem create samples directory and copy sample files there
mkdir docs\b1c\samples
copy ..\..\b1c\docs\samples\*.* docs\b1c\samples
mkdir docs\c1stm8\samples
copy ..\..\c1stm8\docs\samples\*.* docs\c1stm8\samples
mkdir docs\a1stm8\samples
copy ..\..\a1stm8\docs\samples\*.* docs\a1stm8\samples

rem create temp. bin directory and copy binaries
mkdir bin
copy ..\..\bin\win\%platform%\%compiler%\rel\b1c.exe bin
copy ..\..\bin\win\%platform%\%compiler%\rel\c1stm8.exe bin
copy ..\..\bin\win\%platform%\%compiler%\rel\a1stm8.exe bin
rem copy C/C++ run-time libraries (temp. solution: now they should be present in the output directory)
copy ..\..\bin\win\%platform%\%compiler%\rel\*.dll bin

rem create lib directory and copy libraries
mkdir bin\lib
xcopy /e ..\..\common\lib\*.* bin\lib

rem delete output file
if "%build_num%"=="" (
  del %out_file_path%.zip
  set out_file_path=%out_file_path%.zip
) else (
  del %out_file_path%*.zip
  set out_file_path=%out_file_path%-%build_num%.zip
)

rem create distr directory
mkdir ..\..\distr

rem move temp. bin, docs and samples directories to the archive
%path_to_7z% a -sdel -tZIP -- %out_file_path% bin
%path_to_7z% a -sdel -tZIP -- %out_file_path% docs

rem add LICENSE and README.md files to the archive
%path_to_7z% a -tZIP -- %out_file_path% ..\..\LICENSE
%path_to_7z% a -tZIP -- %out_file_path% ..\..\README.md

rem add chengelog
%path_to_7z% a -tZIP -- %out_file_path% ..\..\common\docs\changelog