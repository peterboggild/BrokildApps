# Render the radio script to voice files with the Windows voices, for now (the owner's own recordings replace
# them later, file for file: assets/script/voice/<id>.ogg).
#   powershell -ExecutionPolicy Bypass -File tools\tts.ps1 [script\launch.json]
# Needs ffmpeg on the PATH. Writes assets/script/<name>.json (the script with each line's duration) and the voices.
param([string]$script = "launch.json")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here "script\$script"
$outDir = Join-Path $here "..\assets\script"
$voiceDir = Join-Path $outDir "voice"
New-Item -ItemType Directory -Force $voiceDir | Out-Null
Add-Type -AssemblyName System.Speech
$data = Get-Content $src -Raw -Encoding UTF8 | ConvertFrom-Json
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$tmp = Join-Path $env:TEMP "moon-tts.wav"
$lines = @()
foreach ($l in $data.lines) {
  $v = $data.voices.($l.who)
  $synth.SelectVoice($v.voice)
  $synth.Rate = [int]$v.rate
  $synth.SetOutputToWaveFile($tmp, $fmt)
  $synth.Speak($l.text)
  $synth.SetOutputToNull()
  $p = [double]$v.pitch
  $ogg = Join-Path $voiceDir ($l.id + ".ogg")
  # pitch shift without changing the length, trim the silence the synthesiser leaves at the end, Opus mono
  $af = "asetrate=22050*$p,aresample=22050,atempo=$([math]::Round(1/$p, 4)),silenceremove=stop_periods=-1:stop_duration=0.25:stop_threshold=-50dB"
  & ffmpeg -loglevel error -y -i $tmp -af $af -ac 1 -ar 48000 -c:a libopus -b:a 32k $ogg
  $dur = [double](& ffprobe -v error -show_entries format=duration -of csv=p=0 $ogg)
  $entry = [ordered]@{ id = $l.id; t = $l.t; who = $l.who; text = $l.text; dur = [math]::Round($dur, 2) }
  if ($l.cue) { $entry.cue = $l.cue; $entry.delay = $l.delay }
  if ($l.where) { $entry.where = $l.where }
  $lines += $entry
  Write-Host ("{0,-6} {1,7} {2,-7} {3,5:N2}s  {4}" -f $l.id, $l.t, $l.who, $dur, $l.text)
}
$out = [ordered]@{ about = $data.about; channels = @{}; lines = $lines }
foreach ($k in $data.voices.PSObject.Properties.Name) { $out.channels[$k] = $data.voices.$k.channel }
$name = [IO.Path]::GetFileNameWithoutExtension($src)
($out | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $outDir "$name.json") -Encoding UTF8
Remove-Item $tmp -ErrorAction SilentlyContinue
