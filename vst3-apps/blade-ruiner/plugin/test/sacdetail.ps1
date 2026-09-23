Write-Output ('now: ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))
Write-Output ''
Write-Output '=== full Smart App Control block detail (event 3118) ==='
$ev = Get-WinEvent -LogName 'Microsoft-Windows-CodeIntegrity/Operational' -MaxEvents 60 -ErrorAction SilentlyContinue |
      Where-Object { $_.Id -eq 3118 -or $_.Id -eq 3077 }
$shown = 0
foreach ($e in $ev) {
    if ($shown -ge 2) { break }
    Write-Output ("--- id {0}  {1} ---" -f $e.Id, $e.TimeCreated.ToString('HH:mm:ss'))
    Write-Output $e.Message
    Write-Output ''
    $shown++
}

Write-Output '=== has Defender flagged anything? ==='
$t = Get-MpThreatDetection -ErrorAction SilentlyContinue | Sort-Object InitialDetectionTime -Descending | Select-Object -First 5
if (-not $t) { Write-Output 'no threat detections recorded' }
else { $t | ForEach-Object { Write-Output ("{0}  {1}" -f $_.InitialDetectionTime, ($_.Resources -join ' ')) } }

Write-Output ''
Write-Output '=== signature / mark-of-the-web on each plugin ==='
$root = 'C:\Program Files\Common Files\VST3\Brokild'
foreach ($d in (Get-ChildItem $root -Directory | Where-Object { $_.Name -like '*.vst3' })) {
    $stem = $d.Name.Substring(0, $d.Name.Length - 5)
    $dll  = $d.FullName + '\Contents\x86_64-win\' + $stem + '.vst3'
    if (-not (Test-Path -LiteralPath $dll)) { continue }
    $sig = (Get-AuthenticodeSignature -LiteralPath $dll).Status
    $zone = (Get-Item -LiteralPath $dll -Stream * -ErrorAction SilentlyContinue |
             Where-Object { $_.Stream -eq 'Zone.Identifier' })
    $mi = Test-Path -LiteralPath ($d.FullName + '\Contents\Resources\moduleinfo.json')
    Write-Output ("{0,-16} sig={1,-12} MOTW={2,-6} moduleinfo={3}" -f $stem, $sig, [bool]$zone, $mi)
}
