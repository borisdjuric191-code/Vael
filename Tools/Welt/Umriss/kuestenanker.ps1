# Coast anchors for the painted map (Scripts/Data/Welt/Vael_Karte_Gemalt.json): keeps the hand-set anchors and adds "auto"
# anchors until today's land no longer lands on painted sea. Painted land = outline v9 (teile9, the template the painted map was
# drawn from, same 1024 frame). Run teile9.ps1 first (same env as LIESMICH v9). Then CreateWorldMaps.py and kartenpruefung.ps1.
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot; $root = (Resolve-Path "$d\..\..\..").Path; $data = "$root\Scripts\Data\Welt"
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\regionen5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\gelaendeplan.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kuestenanker.cs"))
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\teile9.png"))
$today = [C5]::Today($game)
$prot = [C5]::Protect($today, 1024, 1024, (New-Object double[] 0), (New-Object double[] 0), 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
$sd = [C5]::Dist($land, 1024, 1024, $true); [void][RG]::Stacks($land, $sd, 1024, 1024, (N $env:FELSEN))
$ins = [KA]::Inside($land, 1024)
$k = Get-Content "$data\Vael_Karte.json" -Raw -Encoding UTF8 | ConvertFrom-Json
[GP]::Min0 = $k.world_min[0]; [GP]::Min1 = $k.world_min[1]; [GP]::Size = $k.world_size
$j = Get-Content "$data\Vael_Karte_Gemalt.json" -Raw -Encoding UTF8 | ConvertFrom-Json
# fixed points: the camps (fast travel) by hand; the other hand-set places only when KEEP_ORTE=1, else the ring fit places them
$campNames = @($k.camps | % { $_.name })
$hand = @($j.anchors | Where-Object { -not $_.auto -and ($env:KEEP_ORTE -eq '1' -or $campNames -contains $_.name) }); $size = [double]$j.size
[KA]::WX = [double[]]($hand | % { $_.world[0] / 1e5 }); [KA]::WY = [double[]]($hand | % { $_.world[1] / 1e5 })
[KA]::U = [double[]]($hand | % { $_.picture[0] / $size }); [KA]::V = [double[]]($hand | % { $_.picture[1] / $size })

# samples: today's land on a 5 px grid (map frame), as world km
$sx = New-Object System.Collections.Generic.List[double]; $sy = New-Object System.Collections.Generic.List[double]; $smx = New-Object System.Collections.Generic.List[int]; $smy = New-Object System.Collections.Generic.List[int]
for ($y = 2; $y -lt 1024; $y += 5) { for ($x = 2; $x -lt 1024; $x += 5) { if (-not $today[$y * 1024 + $x]) { continue }
  $xm = 0.0; $ym = 0.0; [GP]::ToWorld($x, $y, [ref]$xm, [ref]$ym); $sx.Add($xm / 1000); $sy.Add($ym / 1000); $smx.Add($x); $smy.Add($y) } }
"samples $($sx.Count), hand anchors $($hand.Count)"
"before: $([KA]::OnSea($ins, 1024, $sx.ToArray(), $sy.ToArray())) samples on painted sea"
# 1. ring correspondence anchors on a coarse grid of today's land, away from the hand anchors
[double[]]$ring = 440,318, 402,309, 332,304, 262,318, 204,374, 184,458, 184,556, 198,656, 246,748, 348,786, 470,772, 600,734,
  722,694, 824,640, 882,560, 896,478, 890,410, 870,320, 810,300, 740,300, 660,310, 580,318, 510,322, 470,322
$hx = @(); $hy = @(); foreach ($a in $hand) { $mx = 0.0; $my = 0.0; [GP]::ToMap($a.world[0] / 100, $a.world[1] / 100, [ref]$mx, [ref]$my); $hx += $mx; $hy += $my }
$gx = New-Object System.Collections.Generic.List[double]; $gy = New-Object System.Collections.Generic.List[double]
for ($y = 10; $y -lt 1024; $y += [int]$env:RASTER) { for ($x = 10; $x -lt 1024; $x += [int]$env:RASTER) { if (-not $today[$y * 1024 + $x]) { continue }
  $near = $false; for ($h = 0; $h -lt $hx.Count; $h++) { if (($hx[$h] - $x) * ($hx[$h] - $x) + ($hy[$h] - $y) * ($hy[$h] - $y) -lt 26 * 26) { $near = $true; break } }
  if (-not $near) { $gx.Add($x); $gy.Add($y) } } }
$tg = [KA]::RingTargets($ring, $today, $land, 1024, $gx.ToArray(), $gy.ToArray())
$auto = New-Object System.Collections.ArrayList
for ($i = 0; $i -lt $gx.Count; $i++) { if ([double]::IsNaN($tg[2 * $i])) { continue }
  $xm = 0.0; $ym = 0.0; [GP]::ToWorld($gx[$i], $gy[$i], [ref]$xm, [ref]$ym)
  [KA]::Add($xm / 1000, $ym / 1000, $tg[2 * $i] / 1024, $tg[2 * $i + 1] / 1024); [void]$auto.Add(@(($xm * 100), ($ym * 100), ($tg[2 * $i] / 1024 * $size), ($tg[2 * $i + 1] / 1024 * $size))) }
"ring anchors $($auto.Count): $([KA]::OnSea($ins, 1024, $sx.ToArray(), $sy.ToArray())) samples on painted sea"
# 2. a few rounds of single coast anchors for what is left
$log = New-Object 'System.Collections.Generic.List[string]'
$fit = [KA]::Fit($sx.ToArray(), $sy.ToArray(), $smx.ToArray(), $smy.ToArray(), $ins, 1024, [int]$env:RUNDEN, 24, [int]$env:MAXAUTO, $log)
$log
foreach ($a in $fit) { [void]$auto.Add(@(($a[0] * 1e5), ($a[1] * 1e5), ($a[2] * $size), ($a[3] * $size))) }

# write: hand anchors as they were, auto anchors after them
$inv = [Globalization.CultureInfo]::InvariantCulture
$lines = New-Object System.Collections.ArrayList
$allHand = @($j.anchors | Where-Object { -not $_.auto })
foreach ($a in $allHand) { $off = if ($hand -contains $a) { "" } else { '"aus": true, ' }; [void]$lines.Add(('  {{"name": "{0}", {5}"world": [{1}, {2}], "picture": [{3}, {4}]}}' -f $a.name, $a.world[0].ToString($inv), $a.world[1].ToString($inv), $a.picture[0].ToString($inv), $a.picture[1].ToString($inv), $off)) }
foreach ($a in $auto) { [void]$lines.Add(('  {{"name": "Küste", "auto": true, "world": [{0}, {1}], "picture": [{2}, {3}]}}' -f ([Math]::Round($a[0])).ToString($inv), ([Math]::Round($a[1])).ToString($inv), ([Math]::Round($a[2], 1)).ToString($inv), ([Math]::Round($a[3], 1)).ToString($inv))) }
$comment = $j._comment -replace '"', '\"'
$text = "{`n `"_comment`": `"$comment Anchors with auto=true are written by Tools/Welt/Umriss/kuestenanker.ps1 (coast fit), do not edit by hand.`",`n `"picture`": `"$($j.picture)`",`n `"size`": $($j.size),`n `"anchors`": [`n" + ($lines -join ",`n") + "`n ]`n}`n"
$text = $text -replace ' Anchors with auto=true are written by Tools/Welt/Umriss/kuestenanker.ps1 \(coast fit\), do not edit by hand\.( Anchors with auto=true[^"]*)', ' Anchors with auto=true are written by Tools/Welt/Umriss/kuestenanker.ps1 (coast fit), do not edit by hand.'
[IO.File]::WriteAllText("$data\Vael_Karte_Gemalt.json", $text, (New-Object Text.UTF8Encoding $false))
"written: $($hand.Count) hand + $($auto.Count) auto anchors"
