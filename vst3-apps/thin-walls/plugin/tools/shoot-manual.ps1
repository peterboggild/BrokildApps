# Render every page of the handbook to a PNG so it can be LOOKED AT, and run
# the two checks that a render alone will not give you.
#
# THE ONE THAT MATTERS: a .page is a FIXED sheet with overflow:hidden, so
# content that is too tall is not pushed onto a new page - it is silently CUT
# OFF, and the DOM still measures correct. Every element is therefore measured
# against its OWN page's box. Looking at fourteen renders finds this late and
# by luck, and would not catch a page that grows by one line next month.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI and an
# em dash is a parser error.
param(
  [string]$Html = "$PSScriptRoot\..\docs\manual\manual.html",
  [string]$Dest = "$PSScriptRoot\..\docs\manual\pages",
  [int]$Pages = 0,          # 0 = however many sheets the manual actually has
  [int]$Port = 9311
)

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }

# A typed page count goes stale the day the manual grows a sheet, and the plates
# then stop short in silence - three pages of this manual had never been shot.
# Count them in the document instead.
if ($Pages -le 0) {
  $Pages = ([regex]::Matches((Get-Content -Raw $Html), '<section[^>]*class="page')).Count
  if ($Pages -le 0) { throw 'no .page sections found in ' + $Html }
  Write-Output ("  sheets in the manual: " + $Pages)
}

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Get-ChildItem $Dest -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

# 297mm x 210mm at 96 dpi is the page at 1:1, so every plate lands at the size
# it will print. Chrome spends part of the window on its own frame, so ask for
# more and let the extra be dark.
$w = 1123 + 40
$h = 794 + 130

# A headless Chrome left over from a failed run still holds the debugging port,
# and cdp.js then connects to IT - so the gate silently measures the PREVIOUS
# version of the document and reports a fault that has already been fixed.
# That happened here. Kill anything on this port's profile first.
Get-Process chrome -ErrorAction SilentlyContinue |
  Where-Object { $_.MainWindowTitle -eq '' } |
  Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 600

$udd = Join-Path $env:TEMP ('tw-manual-' + (Get-Random))
$cArgs = @('--headless=new', '--disable-gpu', '--no-first-run',
           '--no-default-browser-check', '--hide-scrollbars',
           '--allow-file-access-from-files',
           ('--remote-debugging-port=' + $Port),
           '--remote-allow-origins=*',
           ('--window-size=' + $w + ',' + $h),
           ('"--user-data-dir=' + $udd + '"'),
           ('"file:///' + ($Html -replace '\\','/') + '"')) -join ' '

$proc = Start-Process -FilePath $chrome -ArgumentList $cArgs -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3

$jobs = New-Object System.Collections.ArrayList
[void]$jobs.Add(@{ name = 'settle'; wait = 1500 })

# The window is taller than one page, so scrolling to the LAST page's top is
# clamped by the end of the document and that page is shot offset by the
# difference - which reads as a broken layout when the layout is fine. A
# spacer past the end gives the scroll somewhere to go.
[void]$jobs.Add(@{ name = 'room to scroll the last page to the top'
                   eval = 'var s=document.createElement("div"); s.style.height="600px"; document.body.appendChild(s); "spacer added"' })

[void]$jobs.Add(@{ name = 'pages found'; eval = 'document.querySelectorAll(".page").length + " pages"' })
[void]$jobs.Add(@{ name = 'images loaded'
                   eval = '(function(){var a=[].slice.call(document.images);return a.filter(function(i){return i.complete&&i.naturalWidth>0;}).length+" of "+a.length+" images";})()' })
[void]$jobs.Add(@{ name = 'page is exactly one sheet wide'
                   eval = '"page width " + document.querySelector(".page").getBoundingClientRect().width.toFixed(1) + " in window " + innerWidth' })

$overflow = '(function(){' +
  'var out=[];' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var pr=pg.getBoundingClientRect(), wb=0, wr=0, nb="", nr="";' +
    'pg.querySelectorAll("*").forEach(function(el){' +
      'var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;' +
      'var ob=r.bottom-pr.bottom, orr=r.right-pr.right;' +
      'var who=el.tagName.toLowerCase()+"."+String(el.className||"").split(" ")[0];' +
      'if(ob>wb){wb=ob;nb=who;} if(orr>wr){wr=orr;nr=who;}' +
    '});' +
    'if(wb>1||wr>1) out.push("page "+(i+1)+": "+(wb>1?("BOTTOM +"+wb.toFixed(0)+"px ("+nb+") "):"")+(wr>1?("RIGHT +"+wr.toFixed(0)+"px ("+nr+")"):""));' +
  '});' +
  'return out.length?out.join("  |  "):"clean - every element fits its page";' +
'})()'
[void]$jobs.Add(@{ name = 'OVERFLOW: nothing may fall off its own page'; eval = $overflow })

# The folio is the last thing on a page and the easiest to bury under a column
# that grew. Test whatever carries text of ITS OWN - a wrapper would otherwise
# report for all its children, and testing leaves alone exempts a paragraph
# that happens to contain a link, which is how page 16 ran under the folio
# for a whole release without the gate saying a word.
$folio = '(function(){' +
  'var bad=[];' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var f=pg.querySelector(".folio"); if(!f) return;' +
    'var fr=f.getBoundingClientRect();' +
    'pg.querySelectorAll(".pad *").forEach(function(el){' +
      'var own=false; el.childNodes.forEach(function(n){if(n.nodeType===3&&n.nodeValue.trim())own=true;});' +
      'if(!own) return;' +
      'var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;' +
      'if(r.bottom>fr.top+1 && r.top<fr.bottom) bad.push("page "+(i+1)+" [" + String(el.textContent||"").trim().slice(0,26) + "]");' +
    '});' +
  '});' +
  'return bad.length?bad.slice(0,8).join(" | "):"clean - folios are clear";' +
'})()'
[void]$jobs.Add(@{ name = 'FOLIO: nothing may overlap the page-number strip'; eval = $folio })

# A plate that failed to load renders as a 0x0 box and every layout check above
# then passes on a page that is missing its picture.
[void]$jobs.Add(@{ name = 'PLATES: every img must have pixels'
                   eval = '(function(){var bad=[].slice.call(document.images).filter(function(i){return !(i.complete&&i.naturalWidth>0);}).map(function(i){return i.getAttribute("src");});return bad.length?("MISSING: "+bad.join(", ")):"clean - all plates loaded";})()' })

for ($i = 0; $i -lt $Pages; $i++) {
  [void]$jobs.Add(@{ name = ('scroll to page ' + ($i + 1))
                     eval = ('(function(){var p=document.querySelectorAll(".page")[' + $i + '];' +
                             'if(!p) return "NO SUCH PAGE";' +
                             'window.scrollTo(0, Math.round(p.offsetTop));' +
                             'return "top " + Math.round(p.offsetTop);})()') })
  [void]$jobs.Add(@{ name = 'settle'; wait = 200 })
  [void]$jobs.Add(@{ name = ('page ' + ($i + 1))
                     shoot = ($Dest.Replace('\','/') + '/p' + ('{0:d2}' -f ($i + 1)) + '.png') })
}

$jobsFile = Join-Path $env:TEMP 'tw-manual-shot-jobs.json'
# PowerShell 5.1's -Encoding UTF8 writes a BOM and JSON.parse chokes on it.
$json = $jobs | ConvertTo-Json -Depth 4
[System.IO.File]::WriteAllText($jobsFile, $json, (New-Object System.Text.UTF8Encoding($false)))

& node "$PSScriptRoot\..\tools\cdp.js" $Port $jobsFile

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host ""
Get-ChildItem $Dest -Filter *.png | ForEach-Object {
  Write-Host ("  {0,-10} {1,5} KB" -f $_.Name, [int]($_.Length / 1KB))
}
