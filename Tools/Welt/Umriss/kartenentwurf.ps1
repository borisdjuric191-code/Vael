# Layout draft for the next Tripo picture, made from the last Tripo map (KARTE): thicker jaws with cliff teeth, the tail spit
# reaching into the mouth, the giant mushrooms moved to the Kammweg in the north-west. Output: entwurf.png (no text, for Tripo)
# and entwurf_beschriftet.png (all game places labelled, for us).
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kartenbild.cs")) -ReferencedAssemblies System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile($env:KARTE); $W = $bmp.Width; $H = $bmp.Height
$p = [KB]::Load($bmp); $orig = [KB]::Load($bmp); $bmp.Dispose()
# 1. jaws: the upper one grows down, the lower one grows up, so both are sturdier and the mouth a narrow, island-strewn sound
[KB]::Stretch($p, $W, $H, 470, 770, 262, 299, 326, 30)
[KB]::Stretch($p, $W, $H, 462, 762, 412, 387, 360, 30)
# 2. giant mushrooms from the middle of the forest up to the Kammweg in the north-west
[KB]::Cut($p, $W, $H, $orig, $W, 222, 450, 62, 48, 118, 286, 1.0, $false, 0, 1, 1)
[KB]::Inpaint($p, $W, $H, 253, 473, 36, 28, 6, -70, 0)
$out = [KB]::Save($p, $W, $H)
$g = [System.Drawing.Graphics]::FromImage($out); $g.SmoothingMode = 'AntiAlias'
$ink = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(235, 30, 32, 26)), 1.4
function P($x, $y) { New-Object System.Drawing.PointF ([float]$x), ([float]$y) }
# 3. the tail: a slim rocky spit from the forest tip into the mouth, ending in a sharp point
$green = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 118, 122, 80))
$spit = @((P 462 318), (P 485 322), (P 508 330), (P 528 337), (P 552 343), (P 530 345), (P 506 343), (P 484 342), (P 462 346))
$g.FillClosedCurve($green, $spit, 'Alternate', 0.3); $g.DrawClosedCurve($ink, $spit, 0.3, 'Alternate')
# 4. cliff teeth along both inner jaw coasts, staggered, pointing into the sound
$rock = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 176, 176, 166))
foreach ($x in 575, 615, 655, 695, 735) { $y = 322 + ($x - 575) * 0.03; $t = @((P ($x - 6) $y), (P ($x + 6) $y), (P ($x + 1) ($y + 13)))
  $g.FillPolygon($rock, $t); $g.DrawPolygon($ink, $t) }
foreach ($x in 595, 635, 675, 715) { $y = 364 - ($x - 595) * 0.02; $t = @((P ($x - 6) $y), (P ($x + 6) $y), (P ($x - 1) ($y - 12)))
  $g.FillPolygon($rock, $t); $g.DrawPolygon($ink, $t) }
$out.Save("$d\entwurf.png")
# 5. labelled copy: every game place where it lies in the world (from the regions map, moved onto this outline)
$lab = $out.Clone(); $g2 = [System.Drawing.Graphics]::FromImage($lab); $g2.SmoothingMode = 'AntiAlias'; $g2.TextRenderingHint = 'AntiAlias'
$f = New-Object System.Drawing.Font "Georgia", 10, ([System.Drawing.FontStyle]::Bold); $fr = New-Object System.Drawing.Font "Georgia", 15, ([System.Drawing.FontStyle]::Bold)
$paper = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(215, 240, 232, 212)); $tx = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(40, 28, 20))
$dotB = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(230, 200, 60, 40)); $nid = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(240, 150, 70, 230))
function Add-Place($name, $x, $y, $kind) { $b = if ($kind -eq 'n') { $script:nid } else { $script:dotB }
  $script:g2.FillEllipse($b, [float]($x - 4), [float]($y - 4), [float]8, [float]8)
  $sz = $script:g2.MeasureString($name, $script:f); $script:g2.FillRectangle($script:paper, [float]($x + 6), [float]($y - 8), [float]($sz.Width), [float]15)
  $script:g2.DrawString($name, $script:f, $script:tx, [float]($x + 6), [float]($y - 9)) }
function Add-Region($name, $x, $y) { $sz = $script:g2.MeasureString($name, $script:fr); $script:g2.FillRectangle($script:paper, [float]($x - $sz.Width / 2), [float]($y - 2), [float]$sz.Width, [float]24)
  $script:g2.DrawString($name, $script:fr, $script:tx, [float]($x - $sz.Width / 2), [float]($y - 2)) }
Add-Region "II · Wurzelforst" 200 238; Add-Region "I · Aschenmark" 300 880; Add-Region "IV · Eisenschlund" 790 860; Add-Region "III · Sturmzinnen" 640 150
Add-Place "Kammweg + Riesenpilze" 140 312; Add-Place "Ewiger Hain · Urbaum" 185 410; Add-Place "Eihöhle" 120 452; Add-Place "Pfuhlmoor (Nid)" 205 560 'n'; Add-Place "Forstsaum" 175 598
Add-Place "Schwanzgrat" 330 300; Add-Place "Drachensicht" 405 318; Add-Place "Die Fänge (Watt)" 520 352
Add-Place "Aschenpass" 205 622; Add-Place "Passwache" 160 648; Add-Place "Predigerlager" 245 652; Add-Place "Krater der Glutkönigin" 205 690; Add-Place "Glutfelder" 92 735
Add-Place "Gildenkontor" 255 745; Add-Place "Kathedrale des Ordens (Nid)" 140 790 'n'; Add-Place "Kesselgrund · Lager der Überlebenden" 250 815; Add-Place "Ruinen von Kesselgrund" 330 840
Add-Place "Schlackenhänge" 420 765; Add-Place "Schlackentor-Stollen" 455 722
Add-Place "Schlackentor-Posten" 555 760; Add-Place "Rippenbögen · Knochenhalle" 640 690; Add-Place "Tiefster Schacht (Nid)" 585 712 'n'; Add-Place "Rippenlager" 780 610
Add-Place "Grubenstadt der Gilde" 780 700; Add-Place "Förderturm" 820 735; Add-Place "Minen der Gilde" 640 790; Add-Place "Kernerz-Stollen" 880 650; Add-Place "Nackenhütte" 890 520; Add-Place "Nackenstieg" 900 440
Add-Place "Lücke · Lückenwacht" 480 270; Add-Place "Kieferklippen · Zahnreihe" 600 375; Add-Place "Kliffhütte" 690 395; Add-Place "Akademie am Auge" 800 265; Add-Place "Auge · Tränenkanal (Nid)" 790 338 'n'
Add-Place "Seherturm des Ordens" 760 215; Add-Place "Hornbiwak" 905 185; Add-Place "Gipfel (Bossarena)" 850 60
$lab.Save("$d\entwurf_beschriftet.png"); "ok"
