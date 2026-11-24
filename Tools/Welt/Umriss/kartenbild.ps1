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
$out = [KB]::Save($p, $W, $H); $out.Save("$d\karte_bearbeitet.png"); "ok"









