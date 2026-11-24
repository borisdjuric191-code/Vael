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
# back and tail only: a little fuller than today, tapering towards the tip at the rope bridge
Limb (Smooth @(@(200,560,175), @(200,455,175), @(232,355,150), @(300,305,110), @(380,296,76), @(425,306,42), @(446,316,20)))
# belly between Aschenmark and Eisenschlund: no pinch at the Schlackentor
Limb (Smooth @(@(360,740,150), @(450,730,170), @(540,720,150)))
# the jaw cliffs and the head are one piece of land
Limb @(@(596,300,60), @(630,310,60))
# lower jaw under the Kieferklippen, the mouth stays open water
Limb (Smooth @(@(640,372,70), @(580,385,46), @(525,392,24), @(505,388,12)))
# horns: today's spires drawn out to the north-east
Limb (Smooth @(@(800,215,60), @(820,150,40), @(845,95,20), @(870,55,8)))
Limb (Smooth @(@(880,235,56), @(915,185,36), @(950,150,16), @(980,135,6)))
# folded wing at the shoulder: three bones as fingers of land, bays of sea between them
Limb (Smooth @(@(925,430,70), @(965,385,44), @(1000,330,22), @(1012,300,10)))
Limb (Smooth @(@(930,440,64), @(975,425,40), @(1008,405,18)))
Limb (Smooth @(@(935,455,60), @(975,470,36), @(1006,492,14)))
Limb (Smooth @(@(928,430,90), @(955,420,60)))
# front leg with claws south of the ribs
Limb (Smooth @(@(740,760,120), @(820,810,86), @(880,835,62)))
Limb @(@(880,835,24), @(926,836,12)); Limb @(@(880,835,24), @(912,866,12)); Limb @(@(880,835,24), @(878,876,12))
# hind leg with claws below the Aschenmark
Limb (Smooth @(@(250,820,120), @(190,868,88), @(140,890,62)))
Limb @(@(140,890,24), @(100,888,13)); Limb @(@(140,890,24), @(108,918,13)); Limb @(@(140,890,24), @(142,928,13))
$b.Save("$d\teile.png"); "ok"
