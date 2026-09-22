@ECHO OFF
ECHO Downloading libbitcoin vs2026 dependencies from NuGet
CALL nuget.exe install ..\vs2026\libbitcoin-node\packages.config
CALL nuget.exe install ..\vs2026\libbitcoin-node-test\packages.config
