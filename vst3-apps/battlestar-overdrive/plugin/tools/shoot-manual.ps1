# Render every page of the manual to a PNG, so it can be LOOKED AT.
#
# There is no PDF rasteriser on this machine, so the pages are shot from the
# same HTML the PDF is printed from, in a window sized to exactly one sheet.
# That is not quite the PDF, but it is the same layout engine at the same page
# size, and it is the only way to see a design fault before it ships.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI and an
# em dash is a parser error.
param(
  [string]$Html = "$PSScriptRoot\..\docs\manual\manual.html",
  [string]$Dest = "$PSScriptRoot\..\docs\manual\pages",
  [int]$Port = 9301
)

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Get-ChildItem $Dest -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

# 297mm x 210mm at 96 dpi. Chrome's own CSS pixel is 1/96 inch, so this is the
# page at 1:1 and every plate lands at the size it will print.
$w = 1123; $h = 794
# Chrome spends ~16px of the window on its own frame, so the page would be
# cropped on the right and bottom. Ask for more and let the extra be dark.
$w = $w + 40; $h = $h + 130

Get-Process chrome -ErrorAction SilentlyContinue |
  Where-Object { $_.CommandLine -like "*manual-shots*" } |
  Stop-Process -Force -ErrorAction SilentlyContinue

$udd = Join-Path $env:TEMP ('manual-shots-' + (Get-Random))
$cArgs = @('--headless=new', '--disable-gpu', '--no-first-run',
           '--no-default-browser-check', '--hide-scrollbars',
           ('--remote-debugging-port=' + $Port),
           '--remote-allow-origins=*',
           ('--window-size=' + $w + ',' + $h),
           ('"--user-data-dir=' + $udd + '"'),
           ('"file:///' + ($Html -replace '\\','/') + '"')) -join ' '

$proc = Start-Process -FilePath $chrome -ArgumentList $cArgs -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3

# Build the jobs file: for each .page, scroll it to the top of the window and
# shoot. Scrolling by the element's OWN offsetTop rather than by a computed
# page height means rounding cannot accumulate down a twelve page document.
$jobs = New-Object System.Collections.ArrayList
[void]$jobs.Add(@{ name = 'settle'; wait = 1200 })

# The window is taller than one page, so scrolling to the LAST page's top is
# clamped by the end of the document and that page gets shot offset by the
# difference - which reads as a broken layout when the layout is fine. A
# spacer past the end gives the scroll somewhere to go.
[void]$jobs.Add(@{ name = 'room to scroll the last page to the top'
                   eval = 'var s=document.createElement("div"); s.style.height="500px"; document.body.appendChild(s); "spacer added"' })

[void]$jobs.Add(@{ name = 'pages found'; eval = 'document.querySelectorAll(".page").length + " pages"' })
[void]$jobs.Add(@{ name = 'images loaded'
                   eval = '(function(){var a=[...document.images];return a.filter(i=>i.complete&&i.naturalWidth>0).length+" of "+a.length+" images";})()' })
[void]$jobs.Add(@{ name = 'no scroll bar / exact page width'
                   eval = '"page width " + document.querySelector(".page").getBoundingClientRect().width.toFixed(1) + " in window " + innerWidth' })

# THE CHECK THAT MATTERS. A .page is a fixed sheet with overflow:hidden, so
# anything too tall is silently CUT OFF rather than pushed onto a new page: it
# looks fine in the DOM and wrong in the PDF. Every element is measured against
# its OWN page's box. Looking at twelve renders finds this late and by luck,
# and would not catch a page that grows by one line next month.
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

# The folio is the last thing on a page and the easiest to bury under a box
# that grew. Only leaf elements are tested, or every wrapper reports as well.
$folio = '(function(){' +
  'var bad=[];' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var f=pg.querySelector(".folio"); if(!f) return;' +
    'var fr=f.getBoundingClientRect();' +
    'pg.querySelectorAll(".pad *").forEach(function(el){' +
      'if(el.children.length) return;' +
      'var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;' +
      'if(r.bottom>fr.top+1 && r.top<fr.bottom) bad.push("page "+(i+1)+" [" + String(el.textContent||"").trim().slice(0,26) + "]");' +
    '});' +
  '});' +
  'return bad.length?bad.slice(0,8).join(" | "):"clean - folios are clear";' +
'})()'
[void]$jobs.Add(@{ name = 'FOLIO: nothing may overlap the page-number strip'; eval = $folio })

for ($i = 0; $i -lt 12; $i++) {
  [void]$jobs.Add(@{ name = ('scroll to page ' + ($i + 1))
                     eval = ('(function(){var p=document.querySelectorAll(".page")[' + $i + '];' +
                             'if(!p) return "NO SUCH PAGE";' +
                             'window.scrollTo(0, Math.round(p.offsetTop));' +
                             'return "top " + Math.round(p.offsetTop);})()') })
  [void]$jobs.Add(@{ name = 'settle'; wait = 200 })
  [void]$jobs.Add(@{ name = ('page ' + ($i + 1))
                     shoot = ($Dest.Replace('\','/') + '/p' + ('{0:d2}' -f ($i + 1)) + '.png') })
}

$jobsFile = Join-Path $env:TEMP 'manual-shot-jobs.json'
# PowerShell 5.1's -Encoding UTF8 writes a BOM, and JSON.parse chokes on it.
$json = $jobs | ConvertTo-Json -Depth 4
[System.IO.File]::WriteAllText($jobsFile, $json, (New-Object System.Text.UTF8Encoding($false)))

& node "$PSScriptRoot\..\tools\cdp.js" $Port $jobsFile

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host ""
Get-ChildItem $Dest -Filter *.png | ForEach-Object {
  Write-Host ("  {0,-10} {1,5} KB" -f $_.Name, [int]($_.Length / 1KB))
}
