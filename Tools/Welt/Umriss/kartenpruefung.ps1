# Check of the painted map anchors: the terrain of today (welt_klein.png) bent through the same thin plate spline as the game
# (UVaelWorldMap::WorldToPainted), drawn as red dots over the painted picture, anchors as yellow rings. Output: kartenpruefung.png
Add-Type -AssemblyName System.Drawing
$d = $PSScriptRoot; $root = Resolve-Path "$d\..\..\.."
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Collections.Generic;
public static class TPS {
  static double K(double r2){ return r2>1e-12 ? 0.5*r2*Math.Log(r2) : 0; }
  public static double[][] Solve(double[] wx,double[] wy,double[] u,double[] v,double s){ int c=wx.Length, n=c+3; var res=new double[2][];
    for(int t=0;t<2;t++){ var A=new double[n,n]; var B=new double[n];
      for(int i=0;i<c;i++){ for(int j=0;j<c;j++) A[i,j]=K((wx[i]-wx[j])*(wx[i]-wx[j])+(wy[i]-wy[j])*(wy[i]-wy[j]))+(i==j?s:0);
        A[i,c]=1; A[i,c+1]=wx[i]; A[i,c+2]=wy[i]; A[c,i]=1; A[c+1,i]=wx[i]; A[c+2,i]=wy[i]; B[i]= t==0?u[i]:v[i]; }
      for(int col=0;col<n;col++){ int p=col; for(int r=col+1;r<n;r++) if(Math.Abs(A[r,col])>Math.Abs(A[p,col])) p=r;
        for(int k=0;k<n;k++){ var tmp=A[col,k]; A[col,k]=A[p,k]; A[p,k]=tmp; } var tb=B[col]; B[col]=B[p]; B[p]=tb;
        for(int r=col+1;r<n;r++){ double f=A[r,col]/A[col,col]; for(int k=col;k<n;k++) A[r,k]-=f*A[col,k]; B[r]-=f*B[col]; } }
      for(int r=n-1;r>=0;r--){ double sum=B[r]; for(int k=r+1;k<n;k++) sum-=A[r,k]*B[k]; B[r]=sum/A[r,r]; } res[t]=B; }
    return res; }
  public static double Eval(double[] w,double[] wx,double[] wy,double x,double y){ int c=wx.Length; double v=w[c]+w[c+1]*x+w[c+2]*y;
    for(int i=0;i<c;i++) v+=w[i]*K((x-wx[i])*(x-wx[i])+(y-wy[i])*(y-wy[i])); return v; }
}
"@
$j = Get-Content "$root\Scripts\Data\Welt\Vael_Karte_Gemalt.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$k = Get-Content "$root\Scripts\Data\Welt\Vael_Karte.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$used = @($j.anchors | Where-Object { -not $_.aus })
$wx = [double[]]($used | % { $_.world[0] / 1e5 }); $wy = [double[]]($used | % { $_.world[1] / 1e5 })
$u = [double[]]($used | % { $_.picture[0] }); $v = [double[]]($used | % { $_.picture[1] })
$sol = [TPS]::Solve($wx, $wy, $u, $v, 0.02)
$pic = New-Object System.Drawing.Bitmap ([System.Drawing.Bitmap]::FromFile("$root\$($j.picture)"))
$g = [System.Drawing.Graphics]::FromImage($pic)
# today's land exactly as the coast tools see it (C5.Today on the north-up map), every 4th pixel
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\kueste5.cs")) -ReferencedAssemblies System.Drawing
Add-Type -TypeDefinition ([IO.File]::ReadAllText("$d\gelaendeplan.cs")) -ReferencedAssemblies System.Drawing
$game = [System.Drawing.Bitmap]::FromFile("$d\welt_klein.png"); $game.RotateFlip('Rotate270FlipNone'); $game = New-Object System.Drawing.Bitmap $game
$today = [C5]::Today($game); [GP]::Min0 = $k.world_min[0]; [GP]::Min1 = $k.world_min[1]; [GP]::Size = $k.world_size
$red = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 230, 30, 30))
for ($my = 1; $my -lt 1024; $my += 4) { for ($mx = 1; $mx -lt 1024; $mx += 4) { if (-not $today[$my * 1024 + $mx]) { continue }
  $xm = 0.0; $ym = 0.0; [GP]::ToWorld($mx, $my, [ref]$xm, [ref]$ym); $X = $xm / 1000; $Y = $ym / 1000
  $u = [TPS]::Eval($sol[0], $wx, $wy, $X, $Y); $v = [TPS]::Eval($sol[1], $wx, $wy, $X, $Y)
  $g.FillRectangle($red, [float]($u - 1.5), [float]($v - 1.5), [float]3, [float]3) } }
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Yellow), 2
foreach ($a in $used) { $g.DrawEllipse($pen, [float]($a.picture[0] - 4), [float]($a.picture[1] - 4), [float]8, [float]8) }
$pic.Save("$d\kartenpruefung.png"); "ok"
