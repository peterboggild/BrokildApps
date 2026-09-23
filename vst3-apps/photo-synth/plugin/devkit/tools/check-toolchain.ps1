# Verifies a Windows machine can build JUCE VST3 plugins.
#   powershell -ExecutionPolicy Bypass -File check-toolchain.ps1
$ErrorActionPreference = 'Continue'
$ok = $true

function Report($name, $good, $detail) {
    $mark = if ($good) { '  OK  ' } else { ' MISS ' }
    Write-Output ("[{0}] {1,-22} {2}" -f $mark, $name, $detail)
    if (-not $good) { $script:ok = $false }
}

Write-Output "HTML -> VST3 devkit: toolchain check`n"

# --- CMake ---
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    $v = (& cmake --version | Select-Object -First 1) -replace 'cmake version ',''
    Report 'CMake' ([version]($v -split '-')[0] -ge [version]'3.22') "$v  ($($cmake.Source))"
} else {
    Report 'CMake' $false 'not on PATH — install from cmake.org and tick "Add to PATH"'
}

# --- Visual Studio with the C++ workload ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName
    $ver = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
    if ($vs) {
        $major = ($ver -split '\.')[0]
        $gen = switch ($major) { '18' { 'Visual Studio 18 2026' } '17' { 'Visual Studio 17 2022' } default { "Visual Studio $major" } }
        Report 'Visual Studio C++' $true "$vs ($ver)"
        Write-Output ("        generator string: -G `"$gen`"")
    } else {
        Report 'Visual Studio C++' $false 'found VS but not the "Desktop development with C++" workload'
    }
} else {
    Report 'Visual Studio C++' $false 'not installed — get Community + "Desktop development with C++"'
}

# --- Git ---
$git = Get-Command git -ErrorAction SilentlyContinue
Report 'Git' ($null -ne $git) $(if ($git) { (& git --version) } else { 'not on PATH' })

# --- JUCE ---
function Test-Juce($p) {
    return ($p -and (Test-Path (Join-Path $p 'CMakeLists.txt')) -and (Test-Path (Join-Path $p 'modules\juce_core')))
}
$juce = @($env:JUCE_DIR, 'C:\AudioDev\JUCE', "$env:USERPROFILE\JUCE", 'C:\JUCE',
          "$env:USERPROFILE\AudioDev\JUCE") | Where-Object { Test-Juce $_ } | Select-Object -First 1
if (-not $juce) {
    # look a little harder before giving up — JUCE is often vendored in a project
    foreach ($root in @("$env:USERPROFILE\AudioDev", 'C:\AudioDev', "$env:USERPROFILE\source", 'C:\b')) {
        if (-not (Test-Path $root)) { continue }
        $hit = Get-ChildItem $root -Directory -Recurse -Depth 4 -Filter 'JUCE' -ErrorAction SilentlyContinue |
               Where-Object { Test-Juce $_.FullName } | Select-Object -First 1
        if ($hit) { $juce = $hit.FullName; break }
    }
}
if ($juce) {
    $line = Get-Content (Join-Path $juce 'CMakeLists.txt') | Select-String -Pattern 'project\(JUCE VERSION ([0-9.]+)' | Select-Object -First 1
    $jv = if ($line) { $line.Matches[0].Groups[1].Value } else { 'unknown' }
    Report 'JUCE' $true "$jv at $juce"
} else {
    Report 'JUCE' $false 'clone it: git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git C:\AudioDev\JUCE'
}

# --- WebView2 runtime (needed for the plugin editor) ---
$wvKeys = @(
  'HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
  'HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
)
$wv = $wvKeys | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($wv) { Report 'WebView2 runtime' $true (Get-ItemProperty $wv).pv }
else     { Report 'WebView2 runtime' $false 'install from developer.microsoft.com/microsoft-edge/webview2/' }

# --- VST3 install folder ---
$vst3 = "$env:CommonProgramFiles\VST3"
Report 'VST3 folder' (Test-Path $vst3) $vst3

# --- a browser, for headless probes / screenshots / PDF ---
$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
Report 'Chromium browser' ($null -ne $chrome) $(if ($chrome) { $chrome } else { 'needed for probes, screenshots and the PDF manual' })

Write-Output ""
if ($ok) { Write-Output "Ready. Build the template to confirm:"
           Write-Output '  cmake -S template -B build -G "Visual Studio 18 2026" -A x64'
           Write-Output '  cmake --build build --config Release' }
else     { Write-Output "Install what is marked MISS above, then run this again." }
