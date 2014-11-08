@ECHO OFF
ECHO.
ECHO Downloading Libbitcoin Node dependencies from NuGet
CALL nuget.exe install ..\vs2013\libbitcoin-node\packages.config
CALL nuget.exe install ..\vs2013\libbitcoin-node-test\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\libbitcoin-node.sln 12
ECHO.
PAUSE