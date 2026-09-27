# Render every page of the 1984 manual to a PNG, so it can be LOOKED AT, and
# measure every element against its own sheet.
#
# There is no PDF rasteriser on this machine, so the pages are shot from the
# same HTML the PDF is printed from, in a window sized to exactly one sheet.
# That is not quite the PDF, but it is the same layout engine at the same page
# size, and it is the only way to see a design fault before it ships.
#
# THE COUNT COMES FROM THE DOCUMENT. A typed page count goes stale the day the
# manual grows, and the plate loop then stops short in silence - three pages of
# Battlestar's handbook were never photographed by anyone for exactly that
# reason.
#
# ASCII only - Windows PowerShell 5.1 reads a UTF-8-no-BOM .ps1 as ANSI and an
# em dash is a parser error.
param(
  [string]$Html = "$PSScriptRoot\..\docs\manual\manual.html",
  [string]$Dest = "$PSScriptRoot\..\docs\manual\pages",
  [int]$Port = 9321
)

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") |
          Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $chrome) { throw 'Chrome not found' }
if (-not (Test-Path $Html)) { throw "no manual at $Html" }

# The page count, read out of the file itself.
$src = [System.IO.File]::ReadAllText($Html)
$nPages = ([regex]::Matches($src, '<section class="page">')).Count
if ($nPages -lt 1) { throw 'found no .page sections - has the markup changed?' }
Write-Host ("manual has {0} pages" -f $nPages)

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Get-ChildItem $Dest -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

# 297mm x 210mm at 96 dpi. Chrome's own CSS pixel is 1/96 inch, so this is the
# page at 1:1 and every plate lands at the size it will print.
$w = 1123; $h = 794
# Chrome spends ~16px of the window on its own frame, so the page would be
# cropped on the right and bottom. Ask for more and let the extra be dark.
$w = $w + 40; $h = $h + 130

$udd = Join-Path $env:TEMP ('n84-manual-shots-' + (Get-Random))
$cArgs = @('--headless=new', '--disable-gpu', '--no-first-run',
           '--no-default-browser-check', '--hide-scrollbars',
           ('--remote-debugging-port=' + $Port),
           '--remote-allow-origins=*',
           ('--window-size=' + $w + ',' + $h),
           ('"--user-data-dir=' + $udd + '"'),
           ('"file:///' + ($Html -replace '\\','/') + '"')) -join ' '

$proc = Start-Process -FilePath $chrome -ArgumentList $cArgs -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3

$jobs = New-Object System.Collections.ArrayList
[void]$jobs.Add(@{ name = 'settle'; wait = 1200 })

# The window is taller than one page, so scrolling to the LAST page's top is
# clamped by the end of the document and that page gets shot offset by the
# difference - which reads as a broken layout when the layout is fine. A
# spacer past the end gives the scroll somewhere to go.
[void]$jobs.Add(@{ name = 'room to scroll the last page to the top'
                   eval = 'var s=document.createElement("div"); s.style.height="500px"; document.body.appendChild(s); "spacer added"' })

[void]$jobs.Add(@{ name = 'pages found'
                   eval = ('document.querySelectorAll(".page").length + " pages in the DOM, ' + $nPages + ' counted in the source"') })
[void]$jobs.Add(@{ name = 'plates: how many are shot yet'
                   eval = '(function(){var a=[...document.images];return a.filter(i=>i.complete&&i.naturalWidth>0).length+" of "+a.length+" images loaded (the layout must hold with NONE of them)";})()' })
[void]$jobs.Add(@{ name = 'no scroll bar / exact page width'
                   eval = '"page width " + document.querySelector(".page").getBoundingClientRect().width.toFixed(1) + " in window " + innerWidth' })

# THE CHECK THAT MATTERS. A .page is a fixed sheet with overflow:hidden, so
# anything too tall is silently CUT OFF rather than pushed onto a new page: it
# looks fine in the DOM and wrong in the PDF. Every element is measured against
# its OWN page's box. Looking at eighteen renders finds this late and by luck,
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
  'return out.length?("FAIL - "+out.join("  |  ")):"clean - every element fits its page";' +
'})()'
[void]$jobs.Add(@{ name = 'OVERFLOW: nothing may fall off its own page'; eval = $overflow })

# The folio is the last thing on a page and the easiest to bury under a column
# that grew.
#
# Testing only LEAF elements exempts a paragraph that contains an inline link
# ENTIRELY, and the trailing text of such a paragraph belongs to no leaf at
# all - two real collisions hid behind that in the Thin Walls handbook and the
# gate said "clean" for both. The rule here is ANYTHING HOLDING A TEXT NODE OF
# ITS OWN, which excludes pure wrappers and catches the paragraph.
$folio = '(function(){' +
  'var bad=[];' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var f=pg.querySelector(".folio"); if(!f) return;' +
    'var fr=f.getBoundingClientRect();' +
    'pg.querySelectorAll(".pad *").forEach(function(el){' +
      'var own=false;' +
      'for(var k=0;k<el.childNodes.length;k++){' +
        'var c=el.childNodes[k];' +
        'if(c.nodeType===3 && String(c.nodeValue).trim().length){own=true;break;}' +
      '}' +
      'var tag=el.tagName.toUpperCase();' +
      'if(!own && tag!=="IMG" && tag!=="SVG") return;' +
      'var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;' +
      'if(r.bottom>fr.top+1 && r.top<fr.bottom) bad.push("page "+(i+1)+" [" + (tag==="IMG"?"<img>":String(el.textContent||"").trim().slice(0,26)) + "]");' +
    '});' +
  '});' +
  'return bad.length?("FAIL - "+bad.slice(0,10).join(" | ")):"clean - folios are clear";' +
'})()'
[void]$jobs.Add(@{ name = 'FOLIO: nothing may overlap the page-number strip'; eval = $folio })

# How much room is left on each page, in millimetres, measured against .pad.
# A number beats a guess: without it, trimming a page that overruns is a
# sequence of blind cuts.
$slack = '(function(){' +
  'var out=[];' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var pad=pg.querySelector(".pad"); if(!pad) return;' +
    'var pr=pad.getBoundingClientRect(), bot=pr.top;' +
    'pad.querySelectorAll("*").forEach(function(el){' +
      'var r=el.getBoundingClientRect(); if(r.width===0||r.height===0) return;' +
      'if(r.bottom>bot) bot=r.bottom;' +
    '});' +
    'var mm=(pr.bottom-bot)/96*25.4;' +
    'out.push((i+1)+":"+(mm<0?"OVER ":"")+mm.toFixed(0));' +
  '});' +
  'return out.join("  ");' +
'})()'
[void]$jobs.Add(@{ name = 'SLACK: mm left inside .pad on each page (negative = overrun)'; eval = $slack })

# Every page but the cover must carry a folio, and the numbers must run in
# order. A renumbering slip is invisible in the DOM and obvious in the PDF.
$folios = '(function(){' +
  'var out=[], n=0;' +
  'document.querySelectorAll(".page").forEach(function(pg,i){' +
    'var f=pg.querySelector(".folio .n");' +
    'if(i===0){ if(f) out.push("page 1 carries a folio (the cover should not)"); return; }' +
    'if(!f){ out.push("page "+(i+1)+" has NO folio"); return; }' +
    'var v=parseInt(String(f.textContent).trim(),10);' +
    'if(v!==i+1) out.push("page "+(i+1)+" is numbered "+v);' +
    'n++;' +
  '});' +
  'return out.length?("FAIL - "+out.join(" | ")):("clean - "+n+" folios, numbered in order");' +
'})()'
[void]$jobs.Add(@{ name = 'FOLIO NUMBERS: 02..N, in order, none missing'; eval = $folios })

# A plate slot must reserve its height even with no picture in it, or the
# layout is only checkable once the pictures exist.
$slots = '(function(){' +
  'var bad=[], n=0;' +
  'document.querySelectorAll("img.shot").forEach(function(im){' +
    'var r=im.getBoundingClientRect();' +
    'if(r.height<20) bad.push(im.getAttribute("src")+" is only "+r.height.toFixed(0)+"px tall");' +
    'else n++;' +
  '});' +
  'return bad.length?("FAIL - "+bad.join(" | ")):("clean - "+n+" plate slots hold their height with no image");' +
'})()'
[void]$jobs.Add(@{ name = 'PLATES: a missing picture must not collapse its slot'; eval = $slots })

for ($i = 0; $i -lt $nPages; $i++) {
  [void]$jobs.Add(@{ name = ('scroll to page ' + ($i + 1))
                     eval = ('(function(){var p=document.querySelectorAll(".page")[' + $i + '];' +
                             'if(!p) return "NO SUCH PAGE";' +
                             'window.scrollTo(0, Math.round(p.offsetTop));' +
                             'return "top " + Math.round(p.offsetTop);})()') })
  [void]$jobs.Add(@{ name = 'settle'; wait = 200 })
  [void]$jobs.Add(@{ name = ('page ' + ($i + 1))
                     shoot = ($Dest.Replace('\','/') + '/p' + ('{0:d2}' -f ($i + 1)) + '.png') })
}

$jobsFile = Join-Path $env:TEMP 'n84-manual-shot-jobs.json'
# PowerShell 5.1's -Encoding UTF8 writes a BOM, and JSON.parse chokes on it.
$json = $jobs | ConvertTo-Json -Depth 4
[System.IO.File]::WriteAllText($jobsFile, $json, (New-Object System.Text.UTF8Encoding($false)))

& node "$PSScriptRoot\..\tools\cdp.js" $Port $jobsFile

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Host ""
Get-ChildItem $Dest -Filter *.png | ForEach-Object {
  Write-Host ("  {0,-10} {1,5} KB" -f $_.Name, [int]($_.Length / 1KB))
}
