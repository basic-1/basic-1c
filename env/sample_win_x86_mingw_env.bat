set PATH=%PATH%;C:\build\MinGW-w64\mingw32\bin

REM initialize WXWIN and WXCFG variables for wx-config to work properly
REM comment the next set statements to use system variables

REM set WXWIN=C:\build\wx\wx314
set WXCFG=gcc_dllx86/mswu
set B1_WX_CONFIG=C:\Program Files\CodeLite\wx-config.exe