# Dragon body of vael_karte_v3, each part fitted onto its region of today's world (north-up 1024 px game map)
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
$b = New-Object System.Drawing.Bitmap 1024, 1024
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'AntiAlias'; $g.Clear([System.Drawing.Color]::Black)
$w = [System.Drawing.Brushes]::White
function Pt($x, $y) { New-Object System.Drawing.PointF $x, $y }
# part transform: body point -> game px (scale, rotation in degrees about the body anchor, game anchor)
function Tf([double]$bx, [double]$by, [double]$ax, [double]$ay, [double]$s, [double]$rot, [double]$gx, [double]$gy) {
  $r = $rot * [Math]::PI / 180; $dx = ($bx - $ax) * $s; $dy = ($by - $ay) * $s
  Pt ($gx + $dx * [Math]::Cos($r) - $dy * [Math]::Sin($r)) ($gy + $dx * [Math]::Sin($r) + $dy * [Math]::Cos($r))
}
function PolyT($nums, [double]$ax, [double]$ay, [double]$s, [double]$rot, [double]$gx, [double]$gy) {
  $p = @(); for ($i = 0; $i -lt $nums.Count; $i += 2) { $p += Tf $nums[$i] $nums[$i+1] $ax $ay $s $rot $gx $gy }; $g.FillPolygon($w, $p)
}
function BezPts($s) { $o = @(); for ($k = 0; $k -le 12; $k++) { $t = $k / 12; $u = 1 - $t
  $o += $u*$u*$u*$s[0] + 3*$u*$u*$t*$s[2] + 3*$u*$t*$t*$s[4] + $t*$t*$t*$s[6]
  $o += $u*$u*$u*$s[1] + 3*$u*$u*$t*$s[3] + 3*$u*$t*$t*$s[5] + $t*$t*$t*$s[7] }; return $o }
# thick limb: chain of points with widths, drawn as overlapping discs
function Limb($pts) {
  for ($i = 0; $i -lt $pts.Count - 1; $i++) { $a = $pts[$i]; $c = $pts[$i+1]
    $n = [Math]::Max(2, [int]([Math]::Sqrt(($c[0]-$a[0])*($c[0]-$a[0]) + ($c[1]-$a[1])*($c[1]-$a[1])) / 3))
    for ($k = 0; $k -le $n; $k++) { $t = $k / $n; $x = $a[0] + ($c[0]-$a[0])*$t; $y = $a[1] + ($c[1]-$a[1])*$t; $r = ($a[2] + ($c[2]-$a[2])*$t) / 2
      $g.FillEllipse($w, [float]($x - $r), [float]($y - $r), [float](2*$r), [float](2*$r)) } }
}
function Smooth($pts) {   # Catmull-Rom through [x, y, width] points
  $o = @(); for ($i = 0; $i -lt $pts.Count - 1; $i++) {
    $p0 = $pts[[Math]::Max(0, $i-1)]; $p1 = $pts[$i]; $p2 = $pts[$i+1]; $p3 = $pts[[Math]::Min($pts.Count-1, $i+2)]
    for ($k = 0; $k -lt 8; $k++) { $t = $k / 8; $t2 = $t*$t; $t3 = $t2*$t; $q = @()
      for ($j = 0; $j -lt 3; $j++) { $q += 0.5 * ((2*$p1[$j]) + (-$p0[$j]+$p2[$j])*$t + (2*$p0[$j]-5*$p1[$j]+4*$p2[$j]-$p3[$j])*$t2 + (-$p0[$j]+3*$p1[$j]-3*$p2[$j]+$p3[$j])*$t3) }
      $o += ,$q } }
  $o += ,$pts[$pts.Count-1]; return ,$o
}
function Jit([double]$s, [int]$k) { $r = [Math]::Sin($k * 12.9898 + $s * 78.233) * 43758.5453; $r - [Math]::Floor($r) }
$cut = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0, 255, 0))
function Disc($b, $x, $y, $r) { $g.FillEllipse($b, [float]($x-$r), [float]($y-$r), [float](2*$r), [float](2*$r)) }
# body: the ring route is the spine; slim near the tail, broadest at the belly, narrow at the neck
# v6: the back (Wurzelforst) fuller towards the west, flank and ribs slimmer and pushed outwards so the inner sea grows rounder
$spine = Smooth @(@(440,318,16), @(402,309,52), @(332,300,104), @(258,312,[double]$env:WFW), @(192,374,[double]$env:WFW), @(166,458,[double]$env:WFW),
  @(164,556,[double]$env:WFW), @(176,656,[double]$env:AMW), @(228,752,[double]$env:AMW), @(340,800,[double]$env:AMW), @(470,782,[double]$env:ESW), @(600,746,[double]$env:ESW),
  @(726,700,[double]$env:ESW), @(836,644,[double]$env:ESW), @(900,566,120), @(908,478,100), @(894,410,118))
Limb $spine
# head: wide skull behind the eye, a long snout tapering west over the jaw cliffs
Limb (Smooth @(@(870,320,[double]$env:SKW), @(810,300,[double]$env:SKW), @(740,304,120), @(660,314,92), @(580,320,66), @(510,322,44), @(470,322,30)))
# brow ridge over the eye
Limb (Smooth @(@(840,250,70), @(785,250,46), @(735,262,26)))
# lower jaw, hanging open under the snout; the mouth between stays water and closes round the tail tip
# v7b: tilted up towards the snout and longer, the tip reaches under the snout tip
Limb (Smooth @(@(810,440,70), @(740,430,40), @(660,422,24)))
$wk = $w; $w = $cut; Limb (Smooth @(@(448,398,10), @(540,401,18), @(640,405,20), @(730,410,16), @(780,416,8))); $w = $wk
# v7: horns swept back over the neck, readable but not drawn: a solid root, a sharp rock blade in the sharp channel,
# tips breaking into a trail of skerries; every horn also goes into the snow mask
$snow = New-Object System.Drawing.Bitmap 1024, 1024; $gs = [System.Drawing.Graphics]::FromImage($snow); $gs.SmoothingMode = 'AntiAlias'; $gs.Clear([System.Drawing.Color]::Black)
$sharp = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0, 0, 255))
$horns = @(@(@(880,196,74), @(898,146,56), @(926,100,40), @(962,66,26), @(996,50,14), @(1018,50,5)),
           @(@(950,232,60), @(986,214,42), @(1010,188,24), @(1020,158,9), @(1020,138,3)))
# open water between the two horns, so each reads as its own blade
$wk4 = $w; $w = $cut; Limb (Smooth @(@(1050,110,60), @(1002,134,34), @(968,160,14), @(950,172,4))); $w = $wk4
foreach ($h in $horns) { $k = 0; $sm = Smooth $h
  foreach ($p in $sm) { $k++; if ($k -gt 26 -and (Jit $p[0] $k) -lt 0.3) { continue }
    Disc $w $p[0] $p[1] ($p[2] * 0.4); Disc $sharp $p[0] $p[1] ($p[2] * 0.3)
    $gs.FillEllipse([System.Drawing.Brushes]::White, [float]($p[0] - $p[2] * 0.5), [float]($p[1] - $p[2] * 0.5), [float]$p[2], [float]$p[2]) } }
# snow also on the brow and the crown of the skull
foreach ($p in (Smooth @(@(870,246,22), @(820,240,16), @(770,252,8)))) { $gs.FillEllipse([System.Drawing.Brushes]::White, [float]($p[0] - $p[2] / 2), [float]($p[1] - $p[2] / 2), [float]$p[2], [float]$p[2]) }
# dorsal spines: a chain of narrow, jagged rock islands off the outer coast, laid back towards the tail like spines,
# longest at the shoulders, shrinking down the back; some joined to the coast as thin headlands, gaps between them
$cx = 560; $cy = 520; $k = 0; $n = $spine.Count
$wk2 = $w; $w = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0, 0, 255))
if ($env:ZACKEN -ne '0') { foreach ($i in ($n - 12)..14) { if (($n - 12 - $i) % 5 -ne 0) { continue }
  $p = $spine[$i]; $q = $spine[$i - 3]; $k++
  $dx = $p[0] - $cx; $dy = $p[1] - $cy; $l = [Math]::Sqrt($dx*$dx + $dy*$dy); $nx = $dx / $l; $ny = $dy / $l
  $bx = $q[0] - $p[0]; $by = $q[1] - $p[1]; $bl = [Math]::Sqrt($bx*$bx + $by*$by); $bx /= $bl; $by /= $bl
  $t = ($n - 12 - $i) / ($n - 26.0)                    # 0 at the shoulders, 1 near the tail
  $len = (78 - 54 * $t) * (0.75 + 0.5 * (Jit $p[0] $i)); $wid = (24 - 14 * $t) * (0.8 + 0.4 * (Jit $p[1] $i))
  $gap = if ((Jit ($p[0] + 7) $i) -lt 0.35) { -6 } else { 10 + 8 * (Jit ($p[1] + 3) $i) }
  $x0 = $p[0] + $nx * ($p[2] / 2 + $gap); $y0 = $p[1] + $ny * ($p[2] / 2 + $gap)
  $x1 = $x0 + ($nx * 0.75 + $bx * 0.65) * $len * 0.55; $y1 = $y0 + ($ny * 0.75 + $by * 0.65) * $len * 0.55
  $x2 = $x0 + ($nx * 0.7 + $bx * 0.75) * $len; $y2 = $y0 + ($ny * 0.7 + $by * 0.75) * $len
  if ((Jit ($p[0] + $p[1]) $i) -lt 0.12) { continue }   # a missing spine here and there
  Limb (Smooth @(@($x0, $y0, $wid), @($x1, $y1, $wid * 0.6), @($x2, $y2, 4))) } }
$w = $wk2
# the throat between skull and body on the outer side
$w = $cut; Limb (Smooth @(@(1060,470,150), @(1010,468,80), @(962,466,26))); $w = $wk
# the ring's hole: open inner sea, a slightly irregular oval
# v6: a bigger, rounder inner sea; its rim is broken up below by inner bays
$g.FillEllipse($cut, [float]262, [float]372, [float]590, [float]$env:HOLEH)
foreach ($bay in @(@(@(330,600,70), @(282,640,40), @(262,668,14)), @(@(470,690,80), @(462,730,42), @(458,752,12)),
  @(@(780,560,70), @(828,590,40), @(850,606,12)), @(@(300,450,60), @(262,440,30), @(246,432,8)))) { $w = $cut; Limb (Smooth $bay); $w = $wk }
# outer gulfs and fjords so the ring never reads as a clean oval
$w = $cut
Limb (Smooth @(@(10,582,120), @(95,566,62), @(132,558,22)))
Limb (Smooth @(@(520,995,210), @(530,885,112), @(540,832,40)))
Limb (Smooth @(@(300,945,160), @(302,868,76), @(304,834,24)))
Limb (Smooth @(@(995,790,180), @(915,738,92), @(868,712,30)))
Limb (Smooth @(@(330,222,56), @(330,262,24), @(331,276,8)))
Limb (Smooth @(@(705,975,130), @(710,890,62), @(714,850,20)))
# v6: more notches and small bays along the outer coast
foreach ($n6 in @(@(@(40,420,70), @(80,430,30), @(96,434,8)), @(@(30,700,80), @(80,690,36), @(102,684,10)), @(@(160,900,90), @(196,858,40), @(210,838,10)),
  @(@(410,960,100), @(420,880,40), @(424,852,10)), @(@(620,930,100), @(624,860,40), @(626,830,10)), @(@(830,880,100), @(800,812,40), @(790,790,10)),
  @(@(1010,640,90), @(968,630,40), @(950,626,10)), @(@(250,240,60), @(252,276,26), @(254,292,8)))) { Limb (Smooth $n6) }
$w = $wk
# v7: teeth, small and irregular: upper ones hang from the snout into the mouth, lower ones stand on the jaw
$k = 0; foreach ($x in 500, 528, 552, 580, 606, 634, 660, 690) { $k++; if ((Jit $x $k) -lt 0.2) { continue }
  $len = 8 + 6 * (Jit ($x + 1) $k); $y0 = 390 + ($x - 500) * 0.04
  $wk3 = $w; $w = $sharp; Limb @(@($x, $y0, 11), @(($x + 4), ($y0 + $len), 2)); $w = $wk3 }
$k = 0; foreach ($x in 515, 545, 575, 600, 630, 655, 685, 712) { $k++; if ((Jit ($x + 9) $k) -lt 0.2) { continue }
  $len = 7 + 5 * (Jit ($x + 2) $k); $y0 = 413 + ($x - 515) * 0.07
  $wk3 = $w; $w = $sharp; Limb @(@($x, $y0, 10), @(($x - 4), ($y0 - $len), 2)); $w = $wk3 }
# v7b: broken rock and stumps between the teeth, the mouth reads as a rocky strait
for ($k = 0; $k -lt 26; $k++) { $x = 470 + 280 * (Jit 31 $k); $y = 400 + ($x - 470) * 0.05 + 5 * ((Jit 57 $k) - 0.5); $r = 1.6 + 2.6 * (Jit 83 $k)
  Disc $sharp $x $y $r }
# the lower jaw itself as a narrow rock bar in the sharp channel, so the blur cannot melt it
$wk5 = $w; $w = $sharp; Limb (Smooth @(@(800,436,26), @(725,427,22), @(645,420,17), @(565,415,13), @(495,411,8), @(462,409,4))); $w = $wk5
$snow.Save("$d\schnee7.png")
$b.Save("$d\teile7.png"); "ok"
