Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\teile5.png"))
$s = Get-Content "$d\schutz.json" | ConvertFrom-Json
$today = [C5]::Today($game)
$po = if ($env:ORTE -eq "1") { [double[]]$s.orte } else { New-Object double[] 0 }
$prot = [C5]::Protect($today, 1024, 1024, $po, [double[]]$s.weg, 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
$o = [double[]]$s.orte; $lost = @(); for ($i = 0; $i -lt $o.Count; $i += 2) { if (-not $land[[int]$o[$i+1] * 1024 + [int]$o[$i]]) { $lost += "($([int]$o[$i]),$([int]$o[$i+1]))" } }; "orte im meer: $($lost.Count) $lost"
$c = [C5]::Count($land, $today); "new px $($c[0]) cut px $($c[1])  (1 px = 48 m2)"
$gold = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(60, 44, 30))
$ink = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(70, 58, 46))
$tf = New-Object System.Drawing.Font "Georgia", 24, ([System.Drawing.FontStyle]::Bold)
$sf = New-Object System.Drawing.Font "Georgia", 12
foreach ($show in $false, $true) {
  $img = [C5]::Render($land, $today, 1024, 1024, $show)
  $g = [System.Drawing.Graphics]::FromImage($img); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
  if ($show) {
    $g.DrawString("Umriss v5 über der heutigen Welt", $tf, $gold, 24, 18)
    $g.DrawString("gestrichelt: Küste heute · grün: neues Land · rot: wird Meer (ohne Orte und Wege)", $sf, $ink, 26, 58)
  } else {
    $g.DrawString("Vael", $tf, $gold, 24, 18)
    $g.DrawString("Umriss v5 · Entwurf 24.11.2026 · Norden oben", $sf, $ink, 26, 58)
  }
  $k = 1024 / 7.1; $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(60, 44, 30)), 2
  $g.DrawLine($pen, 30, 990, 30 + $k, 990); $g.DrawString("1 km", $sf, $gold, 40 + $k, 980)
  $name = if ($show) { "v5_vergleich.png" } else { "v5.png" }
  $img.Save("$d\$name")
}
