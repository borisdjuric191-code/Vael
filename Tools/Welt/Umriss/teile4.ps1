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
# body ring from the tail tip round the west, south and east up into the neck; close to today's coast, widest at the belly
$spine = Smooth @(@(440,315,16), @(400,307,40), @(330,305,72), @(255,320,120), @(196,382,166), @(175,462,190),
  @(172,560,226), @(208,676,262), @(290,750,262), @(430,738,290), @(580,718,282), @(720,690,300),
  @(835,630,290), @(875,535,236), @(865,445,176), @(855,350,250))
Limb $spine
# head: a broad wedge over the Sturmzinnen, snout reaching west along the jaw cliffs
Limb (Smooth @(@(850,288,226), @(780,280,262), @(690,300,200), @(620,315,110)))
# lower jaw under the Kieferklippen, the mouth stays open water round the tail tip
Limb (Smooth @(@(660,382,78), @(590,394,50), @(530,398,26), @(505,392,12)))
# horns swept back along the neck, breaking up into ridges and islets towards their tips
foreach ($h in @(@(@(700,215,60), @(760,155,42), @(840,112,22), @(915,96,10)), @(@(820,215,54), @(890,178,34), @(955,164,16), @(1005,168,6)))) {
  $k = 0; foreach ($p in (Smooth $h)) { $k++; if ($k -gt 10 -and (Jit $p[0] $k) -lt 0.3) { continue }
    $r = $p[2] / 2; $g.FillEllipse($w, [float]($p[0]-$r), [float]($p[1]-$r), [float](2*$r), [float](2*$r)) } }
# dorsal crest: a broken row of skerries just off the outer coast, larger towards the neck
$cx = 560; $cy = 520; $k = 0
foreach ($p in $spine) { $k++; if ($k % 3 -ne 0 -or $k -lt 14 -or $k -gt $spine.Count - 10) { continue }
  if ((Jit $p[1] $k) -lt 0.25) { continue }
  $dx = $p[0] - $cx; $dy = $p[1] - $cy; $l = [Math]::Sqrt($dx*$dx + $dy*$dy); $o = $p[2] / 2 + 26 + 14 * (Jit $p[0] $k)
  $x = $p[0] + $dx / $l * $o; $y = $p[1] + $dy / $l * $o; $r = (4 + $p[2] / 34) * (0.6 + 0.7 * (Jit ($p[0]+$p[1]) $k))
  $g.FillEllipse($w, [float]($x-$r), [float]($y-$r), [float](2*$r), [float](2*$r)) }
# cuts (green): gulfs that break the silhouette - the throat between head and body, a fjord between Wurzelforst and Aschenmark, belly bays
$w = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0, 255, 0))
Limb (Smooth @(@(1070,476,170), @(995,470,96), @(948,466,44), @(932,462,16)))
Limb (Smooth @(@(10,578,74), @(80,562,42), @(140,550,12)))
Limb (Smooth @(@(520,990,180), @(530,890,100), @(540,835,36)))
Limb (Smooth @(@(1020,790,160), @(945,756,84), @(906,738,26)))
Limb (Smooth @(@(332,225,56), @(330,262,30), @(330,280,10)))
Limb (Smooth @(@(250,990,150), @(268,895,76), @(278,850,22)))
Limb (Smooth @(@(-10,705,130), @(65,700,64), @(108,698,18)))
# capes (white again): two long, crooked headlands at the belly instead of drawn legs
$w = [System.Drawing.Brushes]::White
Limb (Smooth @(@(800,800,90), @(860,862,52), @(905,918,30), @(925,958,10)))
Limb (Smooth @(@(130,830,80), @(92,890,46), @(70,935,24), @(62,965,8)))
# the ring's hole: the inner sea widened into a rounder oval (places and the ring route stay protected)
if ($env:HOLE -ne "0") { $g.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(0, 255, 0))), [float](545-275), [float](468-165), [float](550), [float](330)) }
$b.Save("$d\teile4.png"); "ok"
