using System; using System.Drawing; using System.Drawing.Imaging; using System.Collections.Generic;
// Coastline v4: ouroboros ring (body drawn as a tapering ring, today only keeps protected content), rougher coast
public static class C5 {
  static int[] Px(Bitmap b){ var r=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
    var a=new int[b.Width*b.Height]; System.Runtime.InteropServices.Marshal.Copy(r.Scan0,a,0,a.Length); b.UnlockBits(r); return a; }
  static double Hash(int x,int y,int s){ unchecked{ int h=x*374761393+y*668265263+s*1442695041; h=(h^(h>>13))*1274126177; return ((h^(h>>16))&0x7fffffff)/(double)0x7fffffff; } }
  static double Val(double x,double y,int s){ int ix=(int)Math.Floor(x), iy=(int)Math.Floor(y); double fx=x-ix, fy=y-iy;
    fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
    double a=Hash(ix,iy,s), b=Hash(ix+1,iy,s), c=Hash(ix,iy+1,s), d=Hash(ix+1,iy+1,s);
    return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy; }
  public static double Fbm(double x,double y,double period,int oct,int s){ double sum=0, amp=1, norm=0, f=1/period;
    for(int o=0;o<oct;o++){ sum+=amp*Val(x*f,y*f,s+o*31); norm+=amp; amp*=0.5; f*=2.03; } return sum/norm; }
  static double Ridge(double x,double y,double period,int oct,int s){ double sum=0, amp=1, norm=0, f=1/period;
    for(int o=0;o<oct;o++){ double v=1-Math.Abs(Val(x*f,y*f,s+o*17)*2-1); sum+=amp*v*v; norm+=amp; amp*=0.5; f*=2.1; } return sum/norm; }
  static float[] Blur(float[] m,int w,int h,int rad){ var t=new float[m.Length]; var o=new float[m.Length];
    for(int y=0;y<h;y++){ float s=0; int n=0; for(int x=-rad;x<w;x++){ if(x+rad<w){s+=m[y*w+x+rad];n++;} if(x-rad-1>=0){s-=m[y*w+x-rad-1];n--;} if(x>=0)t[y*w+x]=s/n; } }
    for(int x=0;x<w;x++){ float s=0; int n=0; for(int y=-rad;y<h;y++){ if(y+rad<h){s+=t[(y+rad)*w+x];n++;} if(y-rad-1>=0){s-=t[(y-rad-1)*w+x];n--;} if(y>=0)o[y*w+x]=s/n; } }
    return o; }
  static void Clean(bool[] m,int w,int h,int minLand,int minSea){ var seen=new bool[m.Length]; var st=new Stack<int>(); var comp=new List<int>();
    for(int s=0;s<m.Length;s++){ if(seen[s])continue; comp.Clear(); st.Push(s); seen[s]=true; bool v=m[s]; bool edge=false;
      while(st.Count>0){ int i=st.Pop(); comp.Add(i); int x=i%w,y=i/w; if(x==0||y==0||x==w-1||y==h-1)edge=true;
        if(x>0&&!seen[i-1]&&m[i-1]==v){seen[i-1]=true;st.Push(i-1);} if(x<w-1&&!seen[i+1]&&m[i+1]==v){seen[i+1]=true;st.Push(i+1);}
        if(y>0&&!seen[i-w]&&m[i-w]==v){seen[i-w]=true;st.Push(i-w);} if(y<h-1&&!seen[i+w]&&m[i+w]==v){seen[i+w]=true;st.Push(i+w);} }
      if(v && comp.Count<minLand) foreach(int i in comp) m[i]=false;
      if(!v && !edge && comp.Count<minSea) foreach(int i in comp) m[i]=true; } }
  public static float[] Dist(bool[] m,int w,int h,bool target){ var d=new float[m.Length]; for(int i=0;i<m.Length;i++) d[i]=m[i]==target?0:1e6f;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int i=y*w+x; if(x>0)d[i]=Math.Min(d[i],d[i-1]+1); if(y>0){d[i]=Math.Min(d[i],d[i-w]+1); if(x>0)d[i]=Math.Min(d[i],d[i-w-1]+1.414f); if(x<w-1)d[i]=Math.Min(d[i],d[i-w+1]+1.414f);} }
    for(int y=h-1;y>=0;y--) for(int x=w-1;x>=0;x--){ int i=y*w+x; if(x<w-1)d[i]=Math.Min(d[i],d[i+1]+1); if(y<h-1){d[i]=Math.Min(d[i],d[i+w]+1); if(x<w-1)d[i]=Math.Min(d[i],d[i+w+1]+1.414f); if(x>0)d[i]=Math.Min(d[i],d[i+w-1]+1.414f);} }
    return d; }
  public static bool[] Today(Bitmap game){ int W=game.Width,H=game.Height; var gp=Px(game); var gm=new bool[W*H];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int c=gp[y*W+x]; int R=(c>>16)&255,G=(c>>8)&255,B=c&255;
      bool sea=Math.Abs(R-72)+Math.Abs(G-98)+Math.Abs(B-107)<30; bool glow=G>R+20&&B>R+20;
      bool frame=x>=60&&x<=445&&y>=570&&y<=912&&R-B<=28&&Math.Abs(R-G)<20; gm[y*W+x]=!(sea||glow||frame); }
    Clean(gm,W,H,3000,0); return gm; }
  // protected land: discs round places and along the walked ring route, plus the deep inside of today's land
  public static bool[] Protect(bool[] today,int W,int H,double[] orte,double[] weg,double rOrt,double rWeg,double deep){
    var p=new bool[W*H]; var inner=Dist(today,W,H,false);
    for(int i=0;i<p.Length;i++){ int x=i%W,y=i/W; p[i]=today[i]&&(inner[i]>deep||(x>=470&&x<=615&&y>=260&&y<=360)); }
    Action<double,double,double> disc=(cx,cy,r)=>{ for(int y=(int)(cy-r);y<=cy+r;y++) for(int x=(int)(cx-r);x<=cx+r;x++){ if(x<0||y<0||x>=W||y>=H)continue; if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=r*r) p[y*W+x]=true; } };
    for(int i=0;i<orte.Length;i+=2) disc(orte[i],orte[i+1],rOrt);
    for(int i=0;i+3<weg.Length;i+=2){ double ax=weg[i],ay=weg[i+1],bx=weg[i+2],by=weg[i+3]; double l=Math.Sqrt((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
      if(l>60) continue;   // jumps between route pieces are no road
      for(double t=0;t<=1;t+=2/Math.Max(l,2)) disc(ax+(bx-ax)*t,ay+(by-ay)*t,rWeg); }
    return p; }
  public static bool[] Land(Bitmap parts,bool[] today,bool[] prot,int W,int H,double big,double mid,double rough,double isles){
    var pp=Px(parts); var f=new float[W*H]; var dp=Dist(prot,W,H,true);   // soft falloff round protected content: coasts hug it instead of biting circles
    for(int i=0;i<f.Length;i++) f[i]=(((pp[i]>>16)&255)/255f)-((((pp[i]>>8)&255)>128&&((pp[i]>>16)&255)<128)?0.9f:0f)+(today[i]?0.15f:0f)+0.8f*Math.Max(0f,1f-dp[i]/40f);
    f=Blur(f,W,H,12);
    var land=new bool[W*H];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
      // big warp: gulfs and headlands a few hundred metres deep; mid warp: fjords and capes
      double wx=x+big*(Fbm(x,y,240,3,11)-0.5)*2+mid*(Fbm(x,y,70,4,13)-0.5)*2+0.4*mid*(Fbm(x,y,24,3,15)-0.5)*2;
      double wy=y+big*(Fbm(x,y,240,3,57)-0.5)*2+mid*(Fbm(x,y,70,4,59)-0.5)*2+0.4*mid*(Fbm(x,y,24,3,61)-0.5)*2;
      int ix=Math.Max(0,Math.Min(W-1,(int)wx)), iy=Math.Max(0,Math.Min(H-1,(int)wy));
      double v=f[iy*W+ix]+rough*(Fbm(x,y,22,5,93)-0.5)+0.4*rough*(Fbm(x,y,7,3,94)-0.5);
      land[y*W+x]=v>0.5||prot[y*W+x]; }
    // archipelagos: islands in clusters off the coast, thinning out towards the open sea
    var ds=Dist(land,W,H,true);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int i=y*W+x; if(land[i]) continue; double d=ds[i]; if(d<8||d>130) continue;
      double cluster=Fbm(x,y,220,3,301); double n=Fbm(x,y,30,5,401);
      if(n > 0.74 - isles*(cluster-0.45) + d/650.0) land[i]=true; }
    // the gap of the rope bridge stays open water
    // a ragged strait between the tail tip and the jaws, crossed by the rope bridge
    for(int y=250;y<=380;y++) for(int x=430;x<=500;x++){ int i=y*W+x; if(today[i]) continue;
      double half=9+10*Fbm(x,y,18,3,71)+Math.Abs(y-318)*0.18; if(Math.Abs(x-464+6*(Fbm(y,0,40,2,72)-0.5))<half) land[i]=false; }
    Clean(land,W,H,18,900); return land; }
  public static Bitmap Render(bool[] land,bool[] today,int W,int H,bool showToday){
    var ds=Dist(land,W,H,true); var dl=Dist(land,W,H,false);
    // region tints from the region centres, blended softly (Aschenmark, Wurzelforst, Sturmzinnen, Eisenschlund, new heart area)
    double[,] rc={{250,720},{200,430},{770,300},{720,640}}; double[,] col={{150,108,72},{98,112,70},{118,122,128},{112,90,74}};
    var height=new float[W*H]; var lf=new float[W*H]; for(int i=0;i<lf.Length;i++) lf[i]=land[i]?1f:0f; lf=Blur(Blur(lf,W,H,14),W,H,10);
    for(int i=0;i<land.Length;i++){ if(!land[i]) continue; int x=i%W,y=i/W; double e=Math.Max(0,Math.Min(1,(lf[i]-0.35)/0.6));
      height[i]=(float)(e*(0.25+0.55*Fbm(x,y,120,6,7)+0.45*Ridge(x,y,160,6,8)*Fbm(x,y,260,2,9))); }
    var o=new Bitmap(W,H,PixelFormat.Format32bppArgb); var r=o.LockBits(new Rectangle(0,0,W,H),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb); var op=new int[W*H];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int i=y*W+x; double R,G,B;
      double paper=Fbm(x,y,9,3,555)-0.5, stain=Fbm(x,y,300,3,556)-0.5;
      if(!land[i]){ double d=ds[i]; double t=Math.Min(1,d/110.0);
        R=96-30*t; G=112-30*t; B=118-28*t; R+=stain*20; G+=stain*20; B+=stain*18;
        if(d<1.6){R=46;G=42;B=38;} else if(Math.Abs(d-5)<0.6||(Math.Abs(d-10)<0.55&&Fbm(x,y,30,2,5)>0.45)){R-=14;G-=12;B-=10;} }
      else { double wsum=0; R=G=B=0;
        for(int k=0;k<4;k++){ double dx=x-rc[k,0], dy=y-rc[k,1]; double wgt=1/Math.Pow(dx*dx+dy*dy+400,1.6); wsum+=wgt; R+=wgt*col[k,0]; G+=wgt*col[k,1]; B+=wgt*col[k,2]; }
        R/=wsum; G/=wsum; B/=wsum;
        double hx=(x>0&&x<W-1)?height[i+1]-height[i-1]:0, hy=(y>0&&y<H-1)?height[i+W]-height[i-W]:0;
        double sh=Math.Max(-1,Math.Min(1,-(hx+hy)*45)); double hgt=height[i];
        R=R*(0.85+0.35*sh)+hgt*40; G=G*(0.85+0.35*sh)+hgt*36; B=B*(0.85+0.35*sh)+hgt*30;
        R+=stain*24; G+=stain*22; B+=stain*18;
        if(dl[i]<1.6){R=52;G=44;B=36;} else if(dl[i]<4){R+=14;G+=12;B+=8;} }
      R+=paper*14; G+=paper*14; B+=paper*12;
      if(showToday){ bool e=false; if(x>0&&y>0&&x<W-1&&y<H-1) e=today[i]!=today[i-1]||today[i]!=today[i+1]||today[i]!=today[i-W]||today[i]!=today[i+W];
        if(e && ((x+y)/5)%2==0){R=250;G=250;B=240;}
        else if(land[i]&&!today[i]){R=R*0.6+60*0.4;G=G*0.6+160*0.4;B=B*0.6+90*0.4;}
        else if(!land[i]&&today[i]){R=R*0.5+210*0.5;G=G*0.5+60*0.5;B=B*0.5+50*0.5;} }
      int ri=(int)Math.Max(0,Math.Min(255,R)), gi=(int)Math.Max(0,Math.Min(255,G)), bi=(int)Math.Max(0,Math.Min(255,B));
      op[i]=unchecked((int)0xFF000000)|(ri<<16)|(gi<<8)|bi; }
    System.Runtime.InteropServices.Marshal.Copy(op,0,r.Scan0,op.Length); o.UnlockBits(r); return o; }
  public static int[] Count(bool[] land,bool[] today){ int add=0,cut=0; for(int i=0;i<land.Length;i++){ if(land[i]&&!today[i])add++; if(!land[i]&&today[i])cut++; } return new int[]{add,cut}; }
}
