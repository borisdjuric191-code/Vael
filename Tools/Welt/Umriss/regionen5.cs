using System; using System.Drawing; using System.Drawing.Imaging;
// Region split for outline v5: every land pixel gets its position s along the ring route (tail tip -> jaw tip);
// regions are cuts across the ring at fixed s values.
public static class RG {
  static double Hash(int x,int y,int s){ unchecked{ int h=x*374761393+y*668265263+s*1442695041; h=(h^(h>>13))*1274126177; return ((h^(h>>16))&0x7fffffff)/(double)0x7fffffff; } }
  static double Val(double x,double y,int s){ int ix=(int)Math.Floor(x), iy=(int)Math.Floor(y); double fx=x-ix, fy=y-iy; fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
    double a=Hash(ix,iy,s), b=Hash(ix+1,iy,s), c=Hash(ix,iy+1,s), d=Hash(ix+1,iy+1,s); return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy; }
  static double Fbm(double x,double y,double period,int oct,int s){ double sum=0, amp=1, norm=0, f=1/period;
    for(int o=0;o<oct;o++){ sum+=amp*Val(x*f,y*f,s+o*31); norm+=amp; amp*=0.5; f*=2.03; } return sum/norm; }
  public static float[] Along(bool[] land,int W,int H,double[] p){ return Along(land,W,H,p,0); }
  // warp > 0 bends the borders: each pixel's position along the ring is shifted by noise, so the cuts wander like
  // ridges and streams instead of straight lines (big bends plus small wiggles)
  public static float[] Along(bool[] land,int W,int H,double[] p,double warp){ int n=p.Length/2; var cum=new double[n];
    for(int k=1;k<n;k++){ double dx=p[2*k]-p[2*k-2], dy=p[2*k+1]-p[2*k-1]; cum[k]=cum[k-1]+Math.Sqrt(dx*dx+dy*dy); }
    var s=new float[W*H];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int i=y*W+x; if(!land[i]){ s[i]=-1; continue; } s[i]=(float)Project(x,y,p,cum);
      if(warp>0) s[i]=(float)Math.Max(0,s[i]+warp*(Fbm(x,y,150,4,301)-0.5)*2+0.35*warp*(Fbm(x,y,30,3,303)-0.5)*2); }
    return s; }
  public static double Project(double x,double y,double[] p,double[] cum){ int n=p.Length/2; double best=1e18, bs=0;
    for(int k=0;k<n-1;k++){ double ax=p[2*k], ay=p[2*k+1], vx=p[2*k+2]-ax, vy=p[2*k+3]-ay; double l2=vx*vx+vy*vy;
      double t=Math.Max(0,Math.Min(1,((x-ax)*vx+(y-ay)*vy)/l2)); double qx=ax+t*vx-x, qy=ay+t*vy-y; double d=qx*qx+qy*qy;
      if(d<best){ best=d; bs=cum[k]+t*Math.Sqrt(l2); } }
    return bs; }
  public static double[] Cum(double[] p){ int n=p.Length/2; var cum=new double[n];
    for(int k=1;k<n;k++){ double dx=p[2*k]-p[2*k-2], dy=p[2*k+1]-p[2*k-1]; cum[k]=cum[k-1]+Math.Sqrt(dx*dx+dy*dy); } return cum; }
  public static int Reg(float s,double[] b){ if(s<0) return -1; int r=0; while(r<b.Length&&s>=b[r]) r++; return r; }
  // s values that split the land into equal shares
  public static double[] Quantiles(float[] s,int parts){ int cnt=0; foreach(var v in s) if(v>=0) cnt++;
    var a=new float[cnt]; int j=0; foreach(var v in s) if(v>=0) a[j++]=v; Array.Sort(a);
    var q=new double[parts-1]; for(int k=1;k<parts;k++) q[k-1]=a[(int)((long)cnt*k/parts)]; return q; }
  public static int[] Areas(float[] s,double[] b){ var a=new int[b.Length+1]; foreach(var v in s){ int r=Reg(v,b); if(r>=0) a[r]++; } return a; }
  public static double[] Centroids(float[] s,double[] b,int W){ var c=new double[3*(b.Length+1)];
    for(int i=0;i<s.Length;i++){ int r=Reg(s[i],b); if(r<0) continue; c[3*r]+=i%W; c[3*r+1]+=i/W; c[3*r+2]++; }
    var o=new double[2*(b.Length+1)]; for(int r=0;r<=b.Length;r++){ o[2*r]=c[3*r]/c[3*r+2]; o[2*r+1]=c[3*r+1]/c[3*r+2]; } return o; }
  // tint regions, solid border for the proposal, dashed light border for today's split
  public static void Overlay(Bitmap img,float[] s,double[] b,double[] bToday,int[] cols){ int W=img.Width,H=img.Height;
    var r=img.LockBits(new Rectangle(0,0,W,H),ImageLockMode.ReadWrite,PixelFormat.Format32bppArgb); var px=new int[W*H];
    System.Runtime.InteropServices.Marshal.Copy(r.Scan0,px,0,px.Length);
    for(int y=1;y<H-1;y++) for(int x=1;x<W-1;x++){ int i=y*W+x; int g=Reg(s[i],b); if(g<0) continue;
      int c=px[i]; double R=(c>>16)&255, G=(c>>8)&255, B=c&255; int k=cols[g];
      R=R*0.62+((k>>16)&255)*0.38; G=G*0.62+((k>>8)&255)*0.38; B=B*0.62+(k&255)*0.38;
      bool edge=false, edgeT=false;
      for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ int j=i+dy*W+dx; if(s[j]<0) continue;
        if(Reg(s[j],b)!=g) edge=true; if(Reg(s[j],bToday)!=Reg(s[i],bToday)) edgeT=true; }
      if(edgeT && ((x+y)/6)%2==0){ R=245; G=240; B=225; }
      if(edge){ R=40; G=28; B=20; }
      px[i]=unchecked((int)0xFF000000)|((int)Math.Min(255,Math.Max(0,R))<<16)|((int)Math.Min(255,Math.Max(0,G))<<8)|(int)Math.Min(255,Math.Max(0,B)); }
    System.Runtime.InteropServices.Marshal.Copy(px,0,r.Scan0,px.Length); img.UnlockBits(r); }
  // v7: sea stacks and skerries: on a jittered grid, one candidate per cell; kept with a chance that falls off with the
  // distance to the coast (dist = distance to land for sea pixels); a few large rocks, many small needles; denser at the head
  static float[] Blur(float[] m,int w,int h,int rad){ var t=new float[m.Length]; var o=new float[m.Length];
    for(int y=0;y<h;y++){ float s=0; int n=0; for(int x=-rad;x<w;x++){ if(x+rad<w){s+=m[y*w+x+rad];n++;} if(x-rad-1>=0){s-=m[y*w+x-rad-1];n--;} if(x>=0)t[y*w+x]=s/n; } }
    for(int x=0;x<w;x++){ float s=0; int n=0; for(int y=-rad;y<h;y++){ if(y+rad<h){s+=t[(y+rad)*w+x];n++;} if(y-rad-1>=0){s-=t[(y-rad-1)*w+x];n--;} if(y>=0)o[y*w+x]=s/n; } }
    return o; }
  public static int Stacks(bool[] land,float[] dist,int W,int H,double dens){ int added=0; int cell=11;
    for(int cy=0;cy<H/cell;cy++) for(int cx=0;cx<W/cell;cx++){
      int x=cx*cell+(int)(Hash(cx,cy,401)*cell), y=cy*cell+(int)(Hash(cx,cy,402)*cell); if(x<4||y<4||x>=W-4||y>=H-4) continue;
      int i=y*W+x; if(land[i]) continue; double d=dist[i]; if(d<5||d>70) continue;
      double head=(x>600&&y<470)?1.8:1.0; double cluster=Fbm(x,y,90,2,405);
      double cl=Math.Max(0,(cluster-0.45)/0.3); double p=dens*head*Math.Exp(-d/16.0)*cl*cl;
      if(Hash(cx,cy,403)>p) continue;
      double u=Hash(cx,cy,404); double r=u<0.85?1.0+1.6*u:3.0+25*(u-0.85);   // mostly needles, now and then a big rock
      double ang=Hash(cx,cy,406)*Math.PI, ex=0.55+0.9*Hash(cx,cy,407);
      int R=(int)Math.Ceiling(r*1.6)+2;
      for(int yy=y-R;yy<=y+R;yy++) for(int xx=x-R;xx<=x+R;xx++){ if(xx<0||yy<0||xx>=W||yy>=H) continue;
        double dx=xx-x, dy=yy-y; double ax=dx*Math.Cos(ang)+dy*Math.Sin(ang), ay=(-dx*Math.Sin(ang)+dy*Math.Cos(ang))/ex;
        double rr=r*(0.75+0.5*Fbm(xx,yy,4,2,408)); if(ax*ax+ay*ay<=rr*rr && !land[yy*W+xx]){ land[yy*W+xx]=true; added++; } } }
    return added; }
  // v7: snow from a painted mask (white = snow), only on land; keeps the relief by scaling with the pixel's own brightness
  // cliffs: on the outer coast, where noise says so, the first pixels inland get a dark cliff face with a light top edge
  public static void Finish(Bitmap img,bool[] land,float[] seaDist,Bitmap snowMask){ int W=img.Width,H=img.Height;
    var r=img.LockBits(new Rectangle(0,0,W,H),ImageLockMode.ReadWrite,PixelFormat.Format32bppArgb); var px=new int[W*H];
    System.Runtime.InteropServices.Marshal.Copy(r.Scan0,px,0,px.Length);
    var sm=new float[W*H]; if(snowMask!=null){ for(int y=0;y<H;y++) for(int x=0;x<W;x++) sm[y*W+x]=snowMask.GetPixel(x,y).R/255f; sm=Blur(sm,W,H,5); }
    for(int y=1;y<H-1;y++) for(int x=1;x<W-1;x++){ int i=y*W+x; if(!land[i]) continue; int c=px[i];
      double R=(c>>16)&255, G=(c>>8)&255, B=c&255; double lum=(R+G+B)/3.0;
      double s=sm[i]*(0.7+0.6*(Fbm(x,y,12,3,501)-0.5)); s=Math.Max(0,Math.Min(1,(s-0.18)/0.4));
      if(s>0){ double v=0.93+(lum-140)/220.0+0.12*(Fbm(x,y,5,2,503)-0.5); v=Math.Max(0.55,Math.Min(1.05,v)); R=R*(1-s)+236*v*s; G=G*(1-s)+241*v*s; B=B*(1-s)+250*v*s; }
      double d=seaDist[i]; double cl=Fbm(x,y,60,3,502);
      if(d<=4 && cl>0.52){ if(d<=1.5){ R*=0.55; G*=0.55; B*=0.55; } else if(d<=3){ R*=0.75; G*=0.75; B*=0.75; } else { R=Math.Min(255,R*1.12); G=Math.Min(255,G*1.12); B=Math.Min(255,B*1.12); } }
      px[i]=unchecked((int)0xFF000000)|((int)Math.Min(255,Math.Max(0,R))<<16)|((int)Math.Min(255,Math.Max(0,G))<<8)|(int)Math.Min(255,Math.Max(0,B)); }
    System.Runtime.InteropServices.Marshal.Copy(px,0,r.Scan0,px.Length); img.UnlockBits(r); }
}