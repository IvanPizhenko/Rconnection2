@echo off
:: Pre-build step for Visual C++ project
:: Parameters: 
:: %1="$(Configuration)"
:: %2="$(OutDir)"
echo Running %~n0...
for /R "%~dp0%~n0.d" %%f in (*.bat) do (
	pushd
	call "%%f" %*
	if errorlevel 1 goto failure
	popd
)
exit /B 0
:failure
popd
exit /B 1
