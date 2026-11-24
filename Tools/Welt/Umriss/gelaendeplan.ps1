# Terrain plan for the new outline (teile9 = outline v9 behind the painted map): where today's land becomes sea, where new
# land comes, and which placed things (camps, places, ores, enemies, settlements, creatures) stand there, with a suggested
# new spot. Run teile9.ps1 first (same env as LIESMICH v9). Writes Docs/Karte/vael_gelaendeplan.png and Docs/Gelaendeplan_Umriss.md
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot; $root = (Resolve-Path "$d\..\..\..").Path; $data = "$root\Scripts\Data\Welt"
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\regionen5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\gelaendeplan.cs")) -ReferencedAssemblies System.Drawing
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\teile9.png"))
$today = [C5]::Today($game)
$prot = [C5]::Protect($today, 1024, 1024, (New-Object double[] 0), (New-Object double[] 0), 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
$sd = [C5]::Dist($land, 1024, 1024, $true); [void][RG]::Stacks($land, $sd, 1024, 1024, (N $env:FELSEN))
$k = Get-Content "$data\Vael_Karte.json" -Raw -Encoding UTF8 | ConvertFrom-Json
[GP]::Min0 = $k.world_min[0]; [GP]::Min1 = $k.world_min[1]; [GP]::Size = $k.world_size
$mpp = [GP]::MetresPerPx(); $km2 = $mpp * $mpp / 1e6

# regions (today's borders) for the area numbers
$union = New-Object bool[] (1024 * 1024); for ($i = 0; $i -lt $union.Length; $i++) { $union[$i] = $today[$i] -or $land[$i] }
[double[]]$ring = 440,318, 402,309, 332,304, 262,318, 204,374, 184,458, 184,556, 198,656, 246,748, 348,786, 470,772, 600,734,
  722,694, 824,640, 882,560, 896,478, 890,410, 870,320, 810,300, 740,300, 660,310, 580,318, 510,322, 470,322
$cum = [RG]::Cum($ring); $along = [RG]::Along($union, 1024, 1024, $ring, 0)
[double[]]$bt = ([RG]::Project(190, 550, $ring, $cum)), ([RG]::Project(435, 760, $ring, $cum)), ([RG]::Project(900, 450, $ring, $cum))
$names = "Wurzelforst", "Aschenmark", "Eisenschlund", "Sturmzinnen"
$lost = @(0, 0, 0, 0); $gain = @(0, 0, 0, 0); $keepA = @(0, 0, 0, 0)
for ($i = 0; $i -lt $union.Length; $i++) { if (-not $union[$i]) { continue }; $r = [RG]::Reg($along[$i], $bt)
  if ($today[$i] -and -not $land[$i]) { $lost[$r]++ } elseif ($land[$i] -and -not $today[$i]) { $gain[$r]++ } else { $keepA[$r]++ } }

# everything placed, in world metres
$items = New-Object System.Collections.ArrayList
function Add-Item($cat, $name, $x, $y) { [void]$script:items.Add([pscustomobject]@{ Cat = $cat; Name = $name; X = [double]$x; Y = [double]$y }) }
foreach ($c in $k.camps) { Add-Item "Lager" $c.name ($c.world[0] / 100) ($c.world[1] / 100) }
foreach ($l in $k.labels) { if ($l.size -lt 2) { Add-Item "Ort" $l.name ($l.world[0] / 100) ($l.world[1] / 100) } }
$erze = Get-Content "$data\AM_Erze.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($v in $erze.vorkommen) { foreach ($s in $v.stuecke) { if ($null -ne $s.x) { Add-Item "Erz" "$($v.art) ($($v.grund))" $s.x $s.y } } }
$geg = Get-Content "$data\AM_Gegner.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($g in $geg.gruppen) { Add-Item "Gegner" "$($g.art -replace 'Vael|Data','') x$($g.anzahl)" $g.x $g.y }
$sied = Get-Content "$data\AM_Siedlungen.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($s in $sied.siedlungen) { Add-Item "Siedlung" $s.name $s.mitte.x $s.mitte.y }
foreach ($f in 'ES_Orte', 'SZ_Orte') { $o = Get-Content "$data\$f.json" -Raw -Encoding UTF8 | ConvertFrom-Json
  foreach ($grp in 'lager', 'orte') { if ($o.$grp) { foreach ($p in $o.$grp.PSObject.Properties) { Add-Item ($(if ($grp -eq 'lager') { 'Lager' } else { 'Ort' })) $p.Name $p.Value.x $p.Value.y } } } }
$wf = Get-Content "$root\Blockout\Vael_Wurzelforst_Blockout\WF_Kreaturen.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($g in $wf.gruppen) { Add-Item "Gegner" "$($g.art) x$($g.anzahl)" ((250000 - $g.ort[1]) / 100) (($g.ort[0] + 175100) / 100) }
# scenery parts of the Aschenmark fill (only counted)
$fill = [IO.File]::ReadAllText("$data\AM_Fuellung.json"); $fillLost = 0; $fillAll = 0
foreach ($m in [regex]::Matches($fill, '\["[^"]+",(-?[\d.]+),(-?[\d.]+),')) { $fillAll++; $mx = 0.0; $my = 0.0
  [GP]::ToMap((N $m.Groups[1].Value), (N $m.Groups[2].Value), [ref]$mx, [ref]$my); if ([GP]::State($today, $land, $mx, $my) -eq 1) { $fillLost++ } }

# check: on new sea, or on land within ~15 m of the new coast
$seaDist = [GP]::Dist($land, $false)
$hits = New-Object System.Collections.ArrayList; $seen = @{}
foreach ($it in $items) { $mx = 0.0; $my = 0.0; [GP]::ToMap($it.X, $it.Y, [ref]$mx, [ref]$my)
  $st = [GP]::State($today, $land, $mx, $my); $ix = [Math]::Max(0, [Math]::Min(1023, [int][Math]::Round($mx))); $iy = [Math]::Max(0, [Math]::Min(1023, [int][Math]::Round($my)))
  $near = $st -eq 0 -and $seaDist[$iy * 1024 + $ix] -le 2
  if ($st -ne 1 -and -not $near) { continue }
  $key = "$($it.Cat)|$($it.Name)|$([int]$it.X)|$([int]$it.Y)"; if ($seen[$key]) { continue }; $seen[$key] = 1
  $r = [RG]::Reg($along[$iy * 1024 + $ix], $bt); $region = if ($r -ge 0) { $names[$r] } else { "?" }
  $best = [GP]::Nearest($today, $land, $seaDist, $mx, $my, 4); $tx = $best % 1024; $ty = [int][Math]::Floor($best / 1024)
  $nx = 0.0; $ny = 0.0; [GP]::ToWorld($tx, $ty, [ref]$nx, [ref]$ny); $dx = $nx - $it.X; $dy = $ny - $it.Y; $dist = [Math]::Sqrt($dx * $dx + $dy * $dy)
  $ang = [Math]::Atan2($dy, $dx) * 180 / [Math]::PI; $dirs = "Norden", "Nordosten", "Osten", "Südosten", "Süden", "Südwesten", "Westen", "Nordwesten"
  $dir = $dirs[([int][Math]::Round((($ang + 360) % 360) / 45)) % 8]
  [void]$hits.Add([pscustomobject]@{ Region = $region; Cat = $it.Cat; Name = $it.Name; X = $it.X; Y = $it.Y; MX = $mx; MY = $my
    Lage = $(if ($st -eq 1) { "wird Meer" } else { "an neuer Küste" }); Weg = [Math]::Round($dist); Richtung = $dir; NX = [Math]::Round($nx); NY = [Math]::Round($ny) }) }

# picture: grey stays, red becomes sea, green new land; numbered rings for the hits (not for each enemy group)
$img = [GP]::Draw($today, $land); $g = [System.Drawing.Graphics]::FromImage($img); $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
$f = New-Object System.Drawing.Font "Arial", 8, ([System.Drawing.FontStyle]::Bold); $tf = New-Object System.Drawing.Font "Georgia", 20, ([System.Drawing.FontStyle]::Bold); $sf = New-Object System.Drawing.Font "Georgia", 11
$white = [System.Drawing.Brushes]::White; $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Yellow), 1.5; $epen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200, 255, 150, 60)), 1
$g.DrawString("Geländeplan: heutiges Land gegen Umriss v9", $tf, $white, 20, 14)
$g.DrawString("grau bleibt Land · rot wird Meer · grün neues Land · gelb: betroffene Orte/Lager (Nummer = Liste) · orange: Gegner, Erze", $sf, $white, 22, 48)
$n = 0; $listed = New-Object System.Collections.ArrayList
foreach ($h in ($hits | Sort-Object Region, Cat, Name)) {
  if ($h.Cat -in 'Gegner', 'Erz') { $g.DrawEllipse($epen, [float]($h.MX - 2.5), [float]($h.MY - 2.5), [float]5, [float]5); continue }
  $n++; $h | Add-Member Nr $n; [void]$listed.Add($h)
  $g.DrawEllipse($pen, [float]($h.MX - 5), [float]($h.MY - 5), [float]10, [float]10); $g.DrawString("$n", $f, $white, [float]($h.MX + 5), [float]($h.MY - 6)) }
$img.Save("$root\Docs\Karte\vael_gelaendeplan.png")

# report
$sb = New-Object System.Text.StringBuilder
$null = $sb.AppendLine("# Geländeplan: das Spielgelände an den neuen Umriss anpassen"); $null = $sb.AppendLine()
$null = $sb.AppendLine("Stand $(Get-Date -Format 'dd.MM.yyyy'), erzeugt von ``Tools/Welt/Umriss/gelaendeplan.ps1``. Vergleich des heutigen Geländes (``welt_klein.png``) mit Umriss v9 (``teile9.ps1``), der Vorlage der gemalten Karte. Bild: ``Docs/Karte/vael_gelaendeplan.png`` (Norden oben). Noch ist **nichts** am Gelände geändert.")
$null = $sb.AppendLine(); $null = $sb.AppendLine("## Flächen"); $null = $sb.AppendLine()
$null = $sb.AppendLine("| Region | bleibt Land | wird Meer | neues Land |"); $null = $sb.AppendLine("|---|---|---|---|")
for ($r = 0; $r -lt 4; $r++) { $null = $sb.AppendLine(("| {0} | {1:N2} km² | {2:N2} km² | {3:N2} km² |" -f $names[$r], ($keepA[$r] * $km2), ($lost[$r] * $km2), ($gain[$r] * $km2))) }
$null = $sb.AppendLine(); $null = $sb.AppendLine("Kulissenteile der Aschenmark (``AM_Fuellung.json``) auf künftigem Meer: $fillLost von $fillAll. Die setzt ``aschenmark_fuellung.py`` beim nächsten Lauf ohnehin neu.")
$null = $sb.AppendLine(); $null = $sb.AppendLine("## Betroffene Lager und Orte"); $null = $sb.AppendLine()
$null = $sb.AppendLine("Vorschlag = nächste Stelle, die heute Land ist und im neuen Umriss mindestens ~25 m von der Küste entfernt liegt (Weltmeter, X Norden, Y Osten).")
$null = $sb.AppendLine(); $null = $sb.AppendLine("| Nr | Region | Art | Name | Lage | heute (X / Y m) | Vorschlag | neu (X / Y m) |"); $null = $sb.AppendLine("|---|---|---|---|---|---|---|---|")
foreach ($h in $listed) { $null = $sb.AppendLine(("| {0} | {1} | {2} | {3} | {4} | {5:N0} / {6:N0} | {7} m nach {8} | {9:N0} / {10:N0} |" -f $h.Nr, $h.Region, $h.Cat, $h.Name, $h.Lage, $h.X, $h.Y, $h.Weg, $h.Richtung, $h.NX, $h.NY)) }
$null = $sb.AppendLine(); $null = $sb.AppendLine("## Gegner (Gruppen, zusammengefasst)"); $null = $sb.AppendLine()
$null = $sb.AppendLine("Gegnergruppen setzen ``aschenmark_gegner.py`` und ``wf_kreaturen.py`` nach Zonen neu; auf künftigem Meer fallen sie einfach weg oder rücken nach. Anzahl der Gruppen:")
$null = $sb.AppendLine(); $null = $sb.AppendLine("| Region | Art | wird Meer | an neuer Küste |"); $null = $sb.AppendLine("|---|---|---|---|")
foreach ($grp in ($hits | Where-Object { $_.Cat -eq 'Gegner' } | Group-Object Region, { $_.Name -replace ' x\d+$', '' } | Sort-Object Name)) {
  $a = $grp.Group[0]; $null = $sb.AppendLine(("| {0} | {1} | {2} | {3} |" -f $a.Region, ($a.Name -replace ' x\d+$', ''), @($grp.Group | Where-Object { $_.Lage -eq 'wird Meer' }).Count, @($grp.Group | Where-Object { $_.Lage -ne 'wird Meer' }).Count)) }
$null = $sb.AppendLine(); $null = $sb.AppendLine("## Erze"); $null = $sb.AppendLine()
$null = $sb.AppendLine("| Region | Erz (Fundort) | Lage | heute (X / Y m) | Vorschlag |"); $null = $sb.AppendLine("|---|---|---|---|---|")
foreach ($h in ($hits | Where-Object { $_.Cat -eq 'Erz' } | Sort-Object Region, Name)) { $null = $sb.AppendLine(("| {0} | {1} | {2} | {3:N0} / {4:N0} | {5} m nach {6} |" -f $h.Region, $h.Name, $h.Lage, $h.X, $h.Y, $h.Weg, $h.Richtung)) }
[IO.File]::WriteAllText("$root\Docs\Gelaendeplan_Umriss.md", $sb.ToString(), (New-Object Text.UTF8Encoding $false))
"items $($items.Count) hits $($hits.Count) listed $($listed.Count)"
for ($r = 0; $r -lt 4; $r++) { "{0,-13} bleibt {1,5:N2}  Meer {2,5:N2}  neu {3,5:N2} km2" -f $names[$r], ($keepA[$r] * $km2), ($lost[$r] * $km2), ($gain[$r] * $km2) }
