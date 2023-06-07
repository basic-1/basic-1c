setlocal enabledelayedexpansion

del "%~1\gitrev.tmp"
del "%~1\gitrev.h"

git rev-list --count HEAD >>"%~1\gitrev.tmp"
if errorlevel 1 goto _fail

set /p rev=<"%~1\gitrev.tmp"
if "%~2"=="next" (set /a rev+=1)

echo #define B1_GIT_REVISION ^"%rev%^">>"%~1\gitrev.h"

goto _success

:_fail
del "%~1\gitrev.h"
echo: >>"%~1\gitrev.h"

:_success
del "%~1\gitrev.tmp"
