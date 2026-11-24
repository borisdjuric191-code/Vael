using System; using System.Drawing; using System.Drawing.Imaging;
// Small paint-over helpers for the Tripo map picture: patch copy with feathered masks, cut-outs against a flat background.
public static class KB {
  public static int[] Load(Bitmap b){ var r=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
    var p=new int[b.Width*b.Height]; System.Runtime.InteropServices.Marshal.Copy(r.Scan0,p,0,p.Length); b.UnlockBits(r); return p; }
  public static Bitmap Save(int[] p,int W,int H){ var b=new Bitmap(W,H,PixelFormat.Format32bppArgb); var r=b.LockBits(new Rectangle(0,0,W,H),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
    System.Runtime.InteropServices.Marshal.Copy(p,0,r.Scan0,p.Length); b.UnlockBits(r); return b; }
  static int R(int c){return (c>>16)&255;} static int G(int c){return (c>>8)&255;} static int B(int c){return c&255;}
  static int Mix(int a,int b,double t){ return unchecked((int)0xFF000000)|((int)(R(a)+(R(b)-R(a))*t)<<16)|((int)(G(a)+(G(b)-G(a))*t)<<8)|(int)(B(a)+(B(b)-B(a))*t); }
  static double Smooth(double e){ e=Math.Max(0,Math.Min(1,e)); return e*e*(3-2*e); }
  // replace an ellipse (feathered by f px) with the picture shifted by (dx,dy)
  public static void Patch(int[] p,int W,int H,double cx,double cy,double rx,double ry,double f,int dx,int dy){ var src=(int[])p.Clone();
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ double u=(x-cx)/rx, v=(y-cy)/ry; double d=Math.Sqrt(u*u+v*v); if(d>1+f/Math.Min(rx,ry)) continue;
      double a=Smooth((1+f/Math.Min(rx,ry)-d)/(f/Math.Min(rx,ry))); int sx=x+dx, sy=y+dy; if(sx<0||sy<0||sx>=W||sy>=H) continue;
      p[y*W+x]=Mix(p[y*W+x],src[sy*W+sx],a); } }
  // paste a box of src (sx,sy,w,h) at (tx,ty) scaled by s, optionally flipped vertically; alpha from the distance to the box's
  // border colour (the flat sea or ground behind the object), so only the drawn object comes along
  public static void Cut(int[] dst,int W,int H,int[] src,int SW,int sx,int sy,int w,int h,double tx,double ty,double s,bool flipV,double lo,double hi,double ell){
    double br=0,bg=0,bb=0; int n=0;
    for(int x=sx;x<sx+w;x++){ foreach(int y in new[]{sy,sy+h-1}){ int c=src[y*SW+x]; br+=R(c); bg+=G(c); bb+=B(c); n++; } }
    for(int y=sy;y<sy+h;y++){ foreach(int x in new[]{sx,sx+w-1}){ int c=src[y*SW+x]; br+=R(c); bg+=G(c); bb+=B(c); n++; } }
    br/=n; bg/=n; bb/=n; int ow=(int)(w*s), oh=(int)(h*s);
    bool ink=lo<0; if(ink) lo=-lo;   // negative lo: alpha from darkness against the background (ink on paper)
    for(int oy=0;oy<oh;oy++) for(int ox=0;ox<ow;ox++){ int X=(int)(tx+ox), Y=(int)(ty+oy); if(X<0||Y<0||X>=W||Y>=H) continue;
      double fx=Math.Min(w-1.001,ox/s), fy=Math.Min(h-1.001,(flipV?(oh-1-oy):oy)/s); int ix=sx+(int)fx, iy=sy+(int)fy; double tx2=fx-(int)fx, ty2=fy-(int)fy;
      int c=Mix(Mix(src[iy*SW+ix],src[iy*SW+ix+1],tx2),Mix(src[(iy+1)*SW+ix],src[(iy+1)*SW+ix+1],tx2),ty2);   // bilinear
      double dist=ink ? ((br+bg+bb)-(R(c)+G(c)+B(c)))/3.0 : Math.Sqrt((R(c)-br)*(R(c)-br)+(G(c)-bg)*(G(c)-bg)+(B(c)-bb)*(B(c)-bb));
      double a=Smooth((dist-lo)/(hi-lo));
      if(ell>0){ double u=(ox-ow/2.0)/(ow/2.0), v=(oy-oh/2.0)/(oh/2.0); double e=Math.Sqrt(u*u+v*v); a=ell>=1? Smooth((1-e)/0.25) : a*Smooth((1-e)/0.25); }
      dst[Y*W+X]=Mix(dst[Y*W+X],c,a); } }
  // fill an ellipse from its surroundings: the smooth colour comes from diffusing the border inwards (so the parchment-to-sea
  // fade continues), the fine texture from the picture shifted by (dx,dy) minus its local average
  public static void Inpaint(int[] p,int W,int H,double cx,double cy,double rx,double ry,double f,int dx,int dy){
    int x0=(int)(cx-rx-f-2), y0=(int)(cy-ry-f-2), x1=(int)(cx+rx+f+2), y1=(int)(cy+ry+f+2); x0=Math.Max(1,x0); y0=Math.Max(1,y0); x1=Math.Min(W-2,x1); y1=Math.Min(H-2,y1);
    int w=x1-x0+1, h=y1-y0+1; var inside=new bool[w*h]; var ch=new double[3][]; for(int k=0;k<3;k++) ch[k]=new double[w*h];
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ double u=(x+x0-cx)/rx, v=(y+y0-cy)/ry; inside[y*w+x]=Math.Sqrt(u*u+v*v)<1+f/Math.Min(rx,ry); int c=p[(y+y0)*W+x+x0];
      ch[0][y*w+x]=R(c); ch[1][y*w+x]=G(c); ch[2][y*w+x]=B(c); }
    for(int it=0;it<1500;it++) for(int k=0;k<3;k++){ var a=ch[k]; for(int y=1;y<h-1;y++) for(int x=1;x<w-1;x++){ int i=y*w+x; if(!inside[i]) continue; a[i]=(a[i-1]+a[i+1]+a[i-w]+a[i+w])/4; } }
    var src=(int[])p.Clone(); int rad=5;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ if(!inside[y*w+x]) continue; int X=x+x0, Y=y+y0;
      double u=(X-cx)/rx, v=(Y-cy)/ry; double d=Math.Sqrt(u*u+v*v); double fr=f/Math.Min(rx,ry); double al=Smooth((1+fr-d)/fr);
      int sx=Math.Max(rad,Math.Min(W-1-rad,X+dx)), sy=Math.Max(rad,Math.Min(H-1-rad,Y+dy)); double mr=0,mg=0,mb=0; int n=0;
      for(int yy=sy-rad;yy<=sy+rad;yy+=2) for(int xx=sx-rad;xx<=sx+rad;xx+=2){ int c2=src[yy*W+xx]; mr+=R(c2); mg+=G(c2); mb+=B(c2); n++; }
      int c=src[sy*W+sx]; double r=ch[0][y*w+x]+R(c)-mr/n, g=ch[1][y*w+x]+G(c)-mg/n, b=ch[2][y*w+x]+B(c)-mb/n;
      int nc=unchecked((int)0xFF000000)|((int)Math.Max(0,Math.Min(255,r))<<16)|((int)Math.Max(0,Math.Min(255,g))<<8)|(int)Math.Max(0,Math.Min(255,b));
      p[Y*W+X]=Mix(p[Y*W+X],nc,al); } }
  // stretch a land strip vertically: between x0 and x1 the row yB (a coast) moves to yC, row yA stays put; tapered over
  // 'tap' px at both ends so the strip grows smoothly (thicker jaws)
  public static void Stretch(int[] p,int W,int H,int x0,int x1,double yA,double yB,double yC,double tap){ var src=(int[])p.Clone();
    for(int x=x0;x<=x1;x++){ double w=Smooth(Math.Min((x-x0)/tap,(x1-x)/tap)); double c=yB+(yC-yB)*w; if(Math.Abs(c-yA)<1) continue;
      int lo=(int)Math.Min(yA,c), hi=(int)Math.Max(yA,c);
      for(int y=lo;y<=hi;y++){ double sy=yA+(y-yA)*(yB-yA)/(c-yA); int s0=(int)Math.Floor(sy); double t=sy-s0; if(s0<0||s0+1>=H) continue;
        p[y*W+x]=Mix(src[s0*W+x],src[(s0+1)*W+x],t); } } }
  // blend an ellipse (feathered) towards a flat colour, keeping a little of the drawing underneath (keep 0..1)
  public static void Clear(int[] p,int W,int H,double cx,double cy,double rx,double ry,double f,int col,double keep){
    for(int y=(int)(cy-ry-f);y<cy+ry+f;y++) for(int x=(int)(cx-rx-f);x<cx+rx+f;x++){ if(x<0||y<0||x>=W||y>=H) continue;
      double u=(x-cx)/rx, v=(y-cy)/ry; double d=Math.Sqrt(u*u+v*v); double fr=f/Math.Min(rx,ry); if(d>1+fr) continue;
      double a=Smooth((1+fr-d)/fr)*(1-keep); p[y*W+x]=Mix(p[y*W+x],col,a); } }
  public static int Avg(int[] p,int W,int x0,int y0,int w,int h){ double r=0,g=0,b=0; int n=0;
    for(int y=y0;y<y0+h;y++) for(int x=x0;x<x0+w;x++){ int c=p[y*W+x]; r+=R(c); g+=G(c); b+=B(c); n++; }
    return unchecked((int)0xFF000000)|((int)(r/n)<<16)|((int)(g/n)<<8)|(int)(b/n); }
  // soft pale glow (screen-like brighten) round a point
  public static void Glow(int[] p,int W,int H,double cx,double cy,double r,int cr,int cg,int cb,double k){
    for(int y=(int)(cy-r);y<cy+r;y++) for(int x=(int)(cx-r);x<cx+r;x++){ if(x<0||y<0||x>=W||y>=H) continue; double d=Math.Sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy))/r; if(d>=1) continue;
      double a=k*(1-d)*(1-d); int c=p[y*W+x]; int r2=(int)(R(c)+(255-R(c))*a*cr/255.0), g2=(int)(G(c)+(255-G(c))*a*cg/255.0), b2=(int)(B(c)+(255-B(c))*a*cb/255.0);
      p[y*W+x]=unchecked((int)0xFF000000)|(r2<<16)|(g2<<8)|b2; } }
  // first sea pixel going from (x,y0) in direction dir (sea = clearly bluer than red)
  public static int Coast(int[] p,int W,int x,int y0,int dir,int lim){ for(int k=0;k<lim;k++){ int c=p[(y0+dir*k)*W+x]; if(B(c)-R(c)>14) return y0+dir*k; } return -1; }
}
