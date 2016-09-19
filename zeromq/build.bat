cd /d "%~dp0"

"%PROGRAMFILES% (x86)\MSBuild\12.0\Bin\MSbuild.exe" "zeromq4-x-4.0.7\builds\msvc\msvc.sln" /t:libzmq /property:configuration=StaticRelease
"%PROGRAMFILES% (x86)\MSBuild\12.0\Bin\MSbuild.exe" "zeromq4-x-4.0.7\builds\msvc\msvc.sln" /t:libzmq /property:configuration=StaticDebug



if exist "..\..\public\" (
	if exist "zeromq4-x-4.0.7\include" (
	echo f | xcopy /Y /Q /S /F "zeromq4-x-4.0.7\include" "..\..\public\include\zmq"
	)
	
	if exist "zeromq4-x-4.0.7\bin\Win32" (
	echo f | xcopy /Y /Q /S /F "zeromq4-x-4.0.7\lib\*.lib" "..\..\public\lib\"
	)
)

if exist "..\..\publish\" (
	if exist "zeromq4-x-4.0.7\include" (
	echo f | xcopy /Y /Q /S /F "zeromq4-x-4.0.7\include" "..\..\publish\zmq"
	)
	
	if exist "zeromq4-x-4.0.7\bin\Win32" (
	echo f | xcopy /Y /Q /S /F "zeromq4-x-4.0.7\lib\*.lib" "..\..\publish\lib\"
	)
)