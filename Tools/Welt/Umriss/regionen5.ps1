Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\regionen5.cs")) -ReferencedAssemblies System.Drawing
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\teile5.png"))
$s = Get-Content "$d\schutz.json" | ConvertFrom-Json
$today = [C5]::Today($game)
$prot = [C5]::Protect($today, 1024, 1024, (New-Object double[] 0), [double[]]$s.weg, 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
# ring route from the tail tip (rope bridge) round the body to the jaw tip, same points as the spine and head in teile5
[double[]]$ring = 440,318, 402,309, 332,304, 262,318, 204,374, 184,458, 184,556, 198,656, 246,748, 348,786, 470,772, 600,734,
  722,694, 824,640, 882,560, 896,478, 890,410, 870,320, 810,300, 740,300, 660,310, 580,318, 510,322, 470,322
$cum = [RG]::Cum($ring)
$along = [RG]::Along($land, 1024, 1024, $ring, $(if ($env:GRENZE) { N $env:GRENZE } else { 0 }))
# today's borders: Aschenpass (Wurzelforst/Aschenmark), Schlackentor (Aschenmark/Eisenschlund), Nackenstieg (Eisenschlund/Sturmzinnen)
[double[]]$bt = ([RG]::Project(190, 550, $ring, $cum)), ([RG]::Project(435, 760, $ring, $cum)), ([RG]::Project(900, 450, $ring, $cum))
$bq = [RG]::Quantiles($along, 4)
$mode = if ($env:TEILUNG) { $env:TEILUNG } else { "gleich" }
[double[]]$b = if ($mode -eq "heute") { $bt } elseif ($mode -eq "mitte") { @(0..2 | % { ($bt[$_] + $bq[$_]) / 2 }) } else { $bq }
$aT = [RG]::Areas($along, $bt); $aB = [RG]::Areas($along, $b)
$names = "Wurzelforst", "Aschenmark", "Eisenschlund", "Sturmzinnen"
for ($r = 0; $r -lt 4; $r++) { "{0,-13} heute {1,5:N2} km2   neu {2,5:N2} km2" -f $names[$r], ($aT[$r] * 48 / 1e6), ($aB[$r] * 48 / 1e6) }
"grenzen heute $($bt -join ' ')  neu $($b -join ' ')"
$img = [C5]::Render($land, $today, 1024, 1024, $false)
[int[]]$cols = 0x5E8A46, 0xD2783A, 0x8C5E46, 0x9CB4D2
[RG]::Overlay($img, $along, $b, $bt, $cols)
$g = [System.Drawing.Graphics]::FromImage($img); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
$ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(40, 28, 20))
$paper = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(200, 240, 232, 215))
$tf = New-Object System.Drawing.Font "Georgia", 24, ([System.Drawing.FontStyle]::Bold)
$rf = New-Object System.Drawing.Font "Georgia", 17, ([System.Drawing.FontStyle]::Bold)
$sf = New-Object System.Drawing.Font "Georgia", 12
$g.DrawString("Vael · Regionen auf Umriss v5", $tf, $ink, 24, 18)
$g.DrawString("durchgezogen: Vorschlag ($mode) · weiß gestrichelt: Grenzen von heute (ungefähr)", $sf, $ink, 26, 58)
$cen = [RG]::Centroids($along, $b, 1024)
$roman = "II", "I", "IV", "III"
for ($r = 0; $r -lt 4; $r++) {
  $t1 = "$($roman[$r]) · $($names[$r])"; $t2 = "{0:N1} km² · {1:N0} %" -f ($aB[$r] * 48 / 1e6), (100.0 * $aB[$r] / ($aB | Measure-Object -Sum).Sum)
  $w1 = $g.MeasureString($t1, $rf); $w2 = $g.MeasureString($t2, $sf); $ww = [Math]::Max($w1.Width, $w2.Width)
  $x = $cen[2*$r] - $ww / 2; $y = $cen[2*$r+1] - 22
  $g.FillRectangle($paper, [float]($x - 6), [float]($y - 2), [float]($ww + 12), [float]44)
  $g.DrawString($t1, $rf, $ink, [float]$x, [float]$y); $g.DrawString($t2, $sf, $ink, [float]($x + ($ww - $w2.Width) / 2), [float]($y + 24))
}
$hf = New-Object System.Drawing.Font "Georgia", 16, ([System.Drawing.FontStyle]::Italic)
if ($env:HERZ -ne "0") { $g.DrawString("V · Das Herz", $hf, $ink, 500, 470) }
$k = 1024 / 7.1; $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(40, 28, 20)), 2
$g.DrawLine($pen, 30, 990, 30 + $k, 990); $g.DrawString("1 km", $sf, $ink, 40 + $k, 980)
$img.Save("$d\v5_regionen_$mode.png")
