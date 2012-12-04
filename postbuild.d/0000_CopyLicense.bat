@echo off
:: Just a placeholder
:: Expects %1 = "$(OutDir)"

echo Copying Rconnection2 license files to the build output dir...

set SRC_DIR=%~dp0..
set DEST_DIR=%~1

for %%F in (Rconnection2.LICENSE Rconnection2.AUTHORS) do (
	echo "  Copying file %%F..."
	copy /B /Y "%SRC_DIR%\%%F" "%DEST_DIR%"
	if errorlevel 1 exit /B 1
)

exit /B 0
