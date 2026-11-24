# Paint-over of the Tripo world map picture (KARTE = input png): remove the northern rock group, rock teeth along the mouth,
# the lone great tree from the first Tripo picture (BAUM = its png) in a clearing in the forest. Output: karte_bearbeitet.png
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kartenbild.cs")) -ReferencedAssemblies System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile($env:KARTE); $W = $bmp.Width; $H = $bmp.Height
$p = [KB]::Load($bmp); $orig = [KB]::Load($bmp); $bmp.Dispose()
$tree = [System.Drawing.Bitmap]::FromFile($env:BAUM); $tp = [KB]::Load($tree); $TW = $tree.Width; $tree.Dispose()
# 1. northern rock group gone: sea from further west on the same rows
[KB]::Inpaint($p, $W, $H, 432, 145, 70, 82, 6, -160, -8)
# second compass rose (bottom right) gone, only the dragon one top left stays
[KB]::Inpaint($p, $W, $H, 920, 902, 64, 88, 6, -125, 22)
# 2. teeth: pointed rocks of that group (boxes in the original), hanging from the upper jaw (flipped) and standing on the
#    lower jaw, staggered; coast lines measured on the picture
$rocks = @(@(429,89,28,28), @(422,120,26,26), @(459,75,23,22), @(437,142,24,19))
$k = 0
foreach ($x in 505, 557, 609) { $r = $rocks[$k % 4]; $k++; $s = 1.05 + 0.15 * ($k % 2)
  $c = 288 + ($x - 500) * 0.12
  [KB]::Cut($p, $W, $H, $orig, $W, $r[0], $r[1], $r[2], $r[3], $x - $r[2] * $s / 2, $c - 4, $s, $true, 12, 32, 0) }
foreach ($x in 531, 583, 640) { $r = $rocks[$k % 4]; $k++; $s = 1.05 + 0.15 * ($k % 2)
  [KB]::Cut($p, $W, $H, $orig, $W, $r[0], $r[1], $r[2], $r[3], $x - $r[2] * $s / 2, 390 - $r[3] * $s, $s, $false, 12, 32, 0) }
# 3. the great tree: a small clearing in the forest, a faint pale glow, the lone ink tree of the first picture on top
$tx = [double]$env:TX; $ty = [double]$env:TY; $ts = [double]$env:TS; $bw = [int]$env:BW; $bh = [int]$env:BH
$col = [KB]::Avg($p, $W, [int]$env:AX, [int]$env:AY, 12, 12)
[KB]::Clear($p, $W, $H, $tx + $bw * $ts / 2, $ty + $bh * $ts * 0.55, $bw * $ts * 0.55, $bh * $ts * 0.45, 18, $col, 0.4)
[KB]::Glow($p, $W, $H, $tx + $bw * $ts / 2, $ty + $bh * $ts * 0.35, $bw * $ts * 0.7, 240, 236, 200, 0.3)
[KB]::Cut($p, $W, $H, $tp, $TW, [int]$env:BX, [int]$env:BY, $bw, $bh, $tx, $ty, $ts, $false, -12, 55, 0)
# 4. the sea serpent in the north-east gone
[KB]::Inpaint($p, $W, $H, 753, 106, 66, 50, 6, -170, 0)
# 5. the mouth: light grey islands of the map itself (boxes in the original) heaped round both jaw tips, so the two
#    peninsulas fray out into skerries instead of ending in open scissor blades
$isl = @{ A = @(740,164,32,46); B = @(497,227,20,20); C = @(528,239,16,16); D = @(572,202,26,28) }
function Isle($n, $cx, $cy, $s, $flip) { $r = $isl[$n]
  [KB]::Cut($p, $W, $H, $orig, $W, $r[0], $r[1], $r[2], $r[3], $cx - $r[2] * $s / 2, $cy - $r[3] * $s / 2, $s, $flip, 12, 32, 0) }
Isle D 452 283 0.85 $false; Isle B 431 276 1.1 $true; Isle C 413 287 1.0 $false; Isle C 448 302 0.8 $true; Isle A 466 263 0.7 $false
Isle A 447 397 0.75 $true; Isle D 427 405 0.85 $false; Isle B 409 395 0.9 $true; Isle C 445 419 0.9 $false; Isle C 471 384 0.8 $true
Isle B 431 341 0.7 $false; Isle C 452 353 0.6 $true
$out = [KB]::Save($p, $W, $H)
$g = [System.Drawing.Graphics]::FromImage($out); $g.SmoothingMode = 'AntiAlias'
function Rnd([double]$s, [int]$k) { $r = [Math]::Sin($k * 12.9898 + $s * 78.233) * 43758.5453; $r - [Math]::Floor($r) }
$ink = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(230, 32, 34, 28)), 1.1
# 6. lights in the crown of the great tree: small soft glowing motes
$ccx = $tx + $bw * $ts / 2; $ccy = $ty + $bh * $ts * 0.3
for ($k = 0; $k -lt 16; $k++) { $a = (Rnd 3 $k) * 2 * [Math]::PI; $rr = [Math]::Sqrt((Rnd 7 $k))
  $x = $ccx + [Math]::Cos($a) * $rr * 36; $y = $ccy + [Math]::Sin($a) * $rr * 22; $sz = 4 + 4 * (Rnd 11 $k)
  for ($j = 3; $j -ge 1; $j--) { $br = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(40 + 50 * (3 - $j), 250, 246, 200))
    $q = $sz * $j / 3; $g.FillEllipse($br, [float]($x - $q / 2), [float]($y - $q / 2), [float]$q, [float]$q) } }
# 7. the moor in the south of the forest: small dark ponds with an ink rim and a pale glint
$water = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(235, 70, 92, 96))
$glint = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(150, 190, 205, 200)), 1
$k = 0; foreach ($pd in @(@(150,540,9,5), @(184,558,6,4), @(214,532,8,5), @(170,584,7,4), @(238,566,6,4), @(198,508,5,3), @(132,566,5,3), @(206,590,5,3), @(160,515,4,3))) { $k++
  $pts = @(); for ($i = 0; $i -lt 9; $i++) { $a = $i / 9 * 2 * [Math]::PI; $f = 0.75 + 0.5 * (Rnd ($pd[0] + $i) $k)
    $pts += New-Object System.Drawing.PointF ([float]($pd[0] + [Math]::Cos($a) * $pd[2] * $f)), ([float]($pd[1] + [Math]::Sin($a) * $pd[3] * $f)) }
  $g.FillClosedCurve($water, $pts); $g.DrawClosedCurve($ink, $pts)
  $g.DrawArc($glint, [float]($pd[0] - $pd[2] * 0.5), [float]($pd[1] - $pd[3] * 0.6), [float]($pd[2] * 0.8), [float]($pd[3] * 0.6), 200, 80) }
# 8. a grove of giant mushrooms in the north-west of the forest
$stem = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(225, 205, 196, 168))
$cap = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(235, 168, 132, 96))
$dot = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(220, 228, 220, 196))
$k = 0; foreach ($m in @(@(250,474,24), @(272,480,16), @(232,484,14), @(286,466,12), @(258,494,11))) { $k++
  $x = $m[0]; $y = $m[1]; $h = $m[2]; $cw = $h * 0.95; $ch = $h * 0.55; $sw = $h * 0.22
  $g.FillRectangle($stem, [float]($x - $sw / 2), [float]($y - $h * 0.55), [float]$sw, [float]($h * 0.55)); $g.DrawRectangle($ink, [float]($x - $sw / 2), [float]($y - $h * 0.55), [float]$sw, [float]($h * 0.55))
  $g.FillPie($cap, [float]($x - $cw / 2), [float]($y - $h * 0.55 - $ch / 2), [float]$cw, [float]$ch, 180, 180)
  $g.DrawPie($ink, [float]($x - $cw / 2), [float]($y - $h * 0.55 - $ch / 2), [float]$cw, [float]$ch, 180, 180)
  for ($i = 0; $i -lt 3; $i++) { $dx = ($i - 1) * $cw * 0.25; $g.FillEllipse($dot, [float]($x + $dx - 1), [float]($y - $h * 0.55 - $ch * 0.28), [float]2, [float]1.6) } }
$out.Save("$d\karte_bearbeitet.png"); "ok"











