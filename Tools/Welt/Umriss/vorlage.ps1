Add-Type -AssemblyName System.Drawing
# Plain template for the painted map (Tripo): region tints, flat sea, snow; no lines, no text. TEILE=teile8 etc.
$d = $PSScriptRoot; $t = $env:TEILE
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\regionen5.cs")) -ReferencedAssemblies System.Drawing
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\$t.png"))
$today = [C5]::Today($game)
$prot = [C5]::Protect($today, 1024, 1024, (New-Object double[] 0), (New-Object double[] 0), 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
$sd = [C5]::Dist($land, 1024, 1024, $true); [void][RG]::Stacks($land, $sd, 1024, 1024, (N $env:FELSEN))
[double[]]$ring = 440,318, 402,309, 332,304, 262,318, 204,374, 184,458, 184,556, 198,656, 246,748, 348,786, 470,772, 600,734,
  722,694, 824,640, 882,560, 896,478, 890,410, 870,320, 810,300, 740,300, 660,310, 580,318, 510,322, 470,322
$cum = [RG]::Cum($ring)
$along = [RG]::Along($land, 1024, 1024, $ring, (N $env:GRENZE))
[double[]]$bt = ([RG]::Project(190, 550, $ring, $cum)), ([RG]::Project(435, 760, $ring, $cum)), ([RG]::Project(900, 450, $ring, $cum))
$bq = [RG]::Quantiles($along, 4)
[double[]]$b = @(0..2 | % { ($bt[$_] + $bq[$_]) / 2 })
# plain template: muted region colours, flat sea, snow, no lines, no text
$cols = @(@(62,84,52), @(118,98,84), @(104,76,58), @(128,138,150))
$snow = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\$($t -replace 'teile', 'schnee').png"))
$img = New-Object System.Drawing.Bitmap 1024, 1024
for ($y = 0; $y -lt 1024; $y++) { for ($x = 0; $x -lt 1024; $x++) { $i = $y * 1024 + $x
  $r = [RG]::Reg($along[$i], $b)
  if ($r -lt 0) { $img.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(70, 98, 112)); continue }
  $c = $cols[$r]; $s = $snow.GetPixel($x, $y).R / 255.0
  $img.SetPixel($x, $y, [System.Drawing.Color]::FromArgb([int]($c[0] + (235 - $c[0]) * $s), [int]($c[1] + (240 - $c[1]) * $s), [int]($c[2] + (248 - $c[2]) * $s))) } }
$img.Save("$d\vorlage_$t.png"); "ok"
