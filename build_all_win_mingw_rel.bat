cd a1stm8\build
call a1stm8_win_x64_mingw_rel.bat %1
cd ..\..

cd b1c\build
call b1c_win_x64_mingw_rel.bat %1
cd ..\..

cd c1stm8\build
call c1stm8_win_x64_mingw_rel.bat %1
cd ..\..

cd a1stm8\build
call a1stm8_win_x86_mingw_rel.bat %1
cd ..\..

cd b1c\build
call b1c_win_x86_mingw_rel.bat %1
cd ..\..

cd c1stm8\build
call c1stm8_win_x86_mingw_rel.bat %1
cd ..\..


cd a1rv32\build
call a1rv32_win_x64_mingw_rel.bat %1
cd ..\..

cd a1rv32\build
call a1rv32_win_x86_mingw_rel.bat %1
cd ..\..
