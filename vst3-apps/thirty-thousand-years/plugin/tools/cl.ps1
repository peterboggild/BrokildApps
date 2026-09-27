# Syntax-check one or more C++ files with MSVC, no build system.
#   powershell -File tools/cl.ps1 Source/Params.cpp Source/Engine.cpp
# ASCII only.
param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Files)
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$root = "$PSScriptRoot\.."
$args2 = ($Files | ForEach-Object { '"' + (Join-Path $root $_) + '"' }) -join " "
$cmd = "`"$vcvars`" >nul && cd /d `"$root`" && cl /nologo /std:c++17 /EHsc /W3 /c /Zs /I Source $args2"
cmd /c $cmd
exit $LASTEXITCODE
