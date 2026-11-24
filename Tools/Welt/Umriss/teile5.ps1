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
$spine = Smooth @(@(440,318,16), @(402,309,40), @(332,304,70), @(262,318,100), @(204,374,132), @(184,458,150),
  @(184,556,150), @(198,656,164), @(246,748,174), @(348,786,180), @(470,772,184), @(600,734,184),
  @(722,694,178), @(824,640,164), @(882,560,134), @(896,478,108), @(890,410,124))
Limb $spine
# head: wide skull behind the eye, a long snout tapering west over the jaw cliffs
Limb (Smooth @(@(870,320,190), @(810,300,200), @(740,300,160), @(660,310,112), @(580,318,78), @(510,322,50), @(470,322,30)))
# brow ridge over the eye
Limb (Smooth @(@(840,245,90), @(780,240,62), @(725,256,36)))
# lower jaw, hanging open under the snout; the mouth between stays water and closes round the tail tip
Limb (Smooth @(@(760,410,90), @(680,408,64), @(600,400,40), @(530,392,22), @(498,384,10)))
$wk = $w; $w = $cut; Limb (Smooth @(@(462,352,8), @(560,352,22), @(660,356,28), @(720,366,20))); $w = $wk
# horns swept back over the neck, east and south-east, breaking up into skerries at the tips
foreach ($h in @(@(@(870,225,60), @(940,225,40), @(995,260,22), @(1020,305,8)), @(@(920,300,50), @(975,340,30), @(995,390,14), @(1000,425,5)))) {
  $k = 0; foreach ($p in (Smooth $h)) { $k++; if ($k -gt 12 -and (Jit $p[0] $k) -lt 0.35) { continue }; Disc $w $p[0] $p[1] ($p[2] / 2) } }
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
$g.FillEllipse($cut, [float]290, [float]385, [float]500, [float]230)
# outer gulfs and fjords so the ring never reads as a clean oval
$w = $cut
Limb (Smooth @(@(10,582,120), @(95,566,62), @(132,558,22)))
Limb (Smooth @(@(520,995,210), @(530,885,112), @(540,832,40)))
Limb (Smooth @(@(300,945,160), @(302,868,76), @(304,834,24)))
Limb (Smooth @(@(995,790,180), @(915,738,92), @(868,712,30)))
Limb (Smooth @(@(330,222,56), @(330,262,24), @(331,276,8)))
Limb (Smooth @(@(705,975,130), @(710,890,62), @(714,850,20)))
$w = $wk
$b.Save("$d\teile5.png"); "ok"
