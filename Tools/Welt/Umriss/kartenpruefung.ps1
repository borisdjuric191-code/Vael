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
public static class SNAP {
  public static int N; public static bool[] L; public static int[] Near;
  public static void Init(string[] rows){ N=rows.Length; L=new bool[N*N]; for(int y=0;y<N;y++) for(int x=0;x<N;x++) L[y*N+x]=rows[y][x]=='1';
    Near=new int[N*N]; for(int i=0;i<Near.Length;i++) Near[i]=-1; var q=new System.Collections.Generic.List<int>();
    for(int y=0;y<N;y++) for(int x=0;x<N;x++){ bool inl=true; for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ int xx=x+dx,yy=y+dy; if(xx<0||yy<0||xx>=N||yy>=N||!L[yy*N+xx]) inl=false; }
      if(inl){ Near[y*N+x]=y*N+x; q.Add(y*N+x); } }
    for(int h=0;h<q.Count;h++){ int i=q[h]; int x=i%N, y=i/N; int[,] nb={{x+1,y},{x-1,y},{x,y+1},{x,y-1}};
      for(int k=0;k<4;k++){ int nx=nb[k,0], ny=nb[k,1]; if(nx<0||ny<0||nx>=N||ny>=N) continue; int o=ny*N+nx; if(Near[o]==-1){ Near[o]=Near[i]; q.Add(o);} } }
    for(int i=0;i<L.Length;i++) if(L[i]) Near[i]=i; }
  // u, v in picture px (0..1024): returns snapped px and whether it was on sea
  public static bool Snap(ref double u,ref double v,int W){ int x=Math.Max(0,Math.Min(N-1,(int)(u/W*N))), y=Math.Max(0,Math.Min(N-1,(int)(v/W*N))); int i=y*N+x;
    if(L[i]||Near[i]<0) return false; int t=Near[i]; u=(t%N+0.5)/N*W; v=(t/N+0.5)/N*W; return true; }
  public static bool IsLand(double u,double v,int W){ int x=Math.Max(0,Math.Min(N-1,(int)(u/W*N))), y=Math.Max(0,Math.Min(N-1,(int)(v/W*N))); return L[y*N+x]; }
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
$red = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 230, 30, 30)); $blue = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(160, 40, 80, 230))
$landRows = [string[]]@([IO.File]::ReadAllLines("$root\Scripts\Data\Welt\Vael_Karte_Gemalt_Land.txt") | Where-Object { $_ }); [SNAP]::Init($landRows); $snapped = 0; $onSea = 0; $all = 0
for ($my = 1; $my -lt 1024; $my += 4) { for ($mx = 1; $mx -lt 1024; $mx += 4) { if (-not $today[$my * 1024 + $mx]) { continue }
  $xm = 0.0; $ym = 0.0; [GP]::ToWorld($mx, $my, [ref]$xm, [ref]$ym); $X = $xm / 1000; $Y = $ym / 1000
  $u = [TPS]::Eval($sol[0], $wx, $wy, $X, $Y); $v = [TPS]::Eval($sol[1], $wx, $wy, $X, $Y)
  $all++; $wasSea = [SNAP]::Snap([ref]$u, [ref]$v, 1024); if ($wasSea) { $snapped++ }; if (-not [SNAP]::IsLand($u, $v, 1024)) { $onSea++ }
  $g.FillRectangle($(if ($wasSea) { $blue } else { $red }), [float]($u - 1.5), [float]($v - 1.5), [float]3, [float]3) } }
"terrain points $all, snapped onto land $snapped, still on painted sea after snapping: $onSea"
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Yellow), 2
foreach ($a in $used) { $g.DrawEllipse($pen, [float]($a.picture[0] - 4), [float]($a.picture[1] - 4), [float]8, [float]8) }
$pic.Save("$d\kartenpruefung.png"); "ok"
