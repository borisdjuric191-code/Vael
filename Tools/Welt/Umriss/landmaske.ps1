# Land mask of the painted map for the game (players are snapped onto painted land): 256 x 256 cells, a cell is land when
# outline v9 (the template the painting was made from) says land AND the painting there is drawn (ink texture), not the
# smooth sea wash. Run teile9.ps1 first (same env as v9). Writes Scripts/Data/Welt/Vael_Karte_Gemalt_Land.txt (256 lines of
# 0/1) and landmaske.png (mask over the painting, for checking).
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot; $root = (Resolve-Path "$d\..\..\..").Path; $data = "$root\Scripts\Data\Welt"
function N($v) { [double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture) }
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\regionen5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing; using System.Drawing.Imaging;
public static class LM {
  public static float[] Texture(Bitmap b){ int W=b.Width; var r=b.LockBits(new Rectangle(0,0,W,W),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
    var p=new int[W*W]; System.Runtime.InteropServices.Marshal.Copy(r.Scan0,p,0,p.Length); b.UnlockBits(r);
    var l=new float[W*W]; for(int i=0;i<p.Length;i++){ int c=p[i]; l[i]=(((c>>16)&255)+((c>>8)&255)+(c&255))/3f; }
    // per 4x4 cell: standard deviation of brightness over the 8x8 around it
    int N=W/4; var t=new float[N*N];
    for(int cy=0;cy<N;cy++) for(int cx=0;cx<N;cx++){ double s=0,s2=0; int n=0;
      for(int y=cy*4-2;y<cy*4+6;y++) for(int x=cx*4-2;x<cx*4+6;x++){ if(x<0||y<0||x>=W||y>=W) continue; double v=l[y*W+x]; s+=v; s2+=v*v; n++; }
      double m=s/n; t[cy*N+cx]=(float)Math.Sqrt(Math.Max(0,s2/n-m*m)); }
    return t; }
  // cells: land if the outline says land for most of the 4x4 and the painting is textured (or a textured neighbour closes a gap)
  public static bool[] Mask(bool[] outline,float[] tex,int W,double thr){ int N=W/4; var m=new bool[N*N];
    for(int cy=0;cy<N;cy++) for(int cx=0;cx<N;cx++){ int c=0; for(int y=0;y<4;y++) for(int x=0;x<4;x++) if(outline[(cy*4+y)*W+cx*4+x]) c++;
      m[cy*N+cx]= c>=8 && tex[cy*N+cx]>=thr; }
    // close small holes: a cell with 6+ land neighbours becomes land, a land cell with <2 land neighbours goes
    var o=new bool[N*N]; for(int cy=0;cy<N;cy++) for(int cx=0;cx<N;cx++){ int n=0; for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ if(dx==0&&dy==0) continue; int x=cx+dx,y=cy+dy; if(x<0||y<0||x>=N||y>=N) continue; if(m[y*N+x]) n++; }
      o[cy*N+cx]= m[cy*N+cx] ? n>=2 : n>=6; }
    return o; }
}
"@
$parts = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$d\teile9.png"))
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$today = [C5]::Today($game)
$prot = [C5]::Protect($today, 1024, 1024, (New-Object double[] 0), (New-Object double[] 0), 26, 16, (N $env:DEEP))
$land = [C5]::Land($parts, $today, $prot, 1024, 1024, (N $env:BIG), (N $env:MID), (N $env:ROUGH), (N $env:ISLES))
$sd = [C5]::Dist($land, 1024, 1024, $true); [void][RG]::Stacks($land, $sd, 1024, 1024, (N $env:FELSEN))
$j = Get-Content "$data\Vael_Karte_Gemalt.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$pic = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$root\$($j.picture)"))
$tex = [LM]::Texture($pic); $mask = [LM]::Mask($land, $tex, 1024, (N $env:TEXTUR))
$sb = New-Object System.Text.StringBuilder; $count = 0
for ($y = 0; $y -lt 256; $y++) { for ($x = 0; $x -lt 256; $x++) { if ($mask[$y * 256 + $x]) { [void]$sb.Append('1'); $count++ } else { [void]$sb.Append('0') } }; [void]$sb.Append("`n") }
[IO.File]::WriteAllText("$data\Vael_Karte_Gemalt_Land.txt", $sb.ToString())
$g = [System.Drawing.Graphics]::FromImage($pic); $br = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(90, 30, 200, 60))
for ($y = 0; $y -lt 256; $y++) { for ($x = 0; $x -lt 256; $x++) { if ($mask[$y * 256 + $x]) { $g.FillRectangle($br, $x * 4, $y * 4, 4, 4) } } }
$pic.Save("$d\landmaske.png"); "land cells $count of 65536"
