using System; using System.Collections.Generic; using System.Drawing; using System.Drawing.Imaging;
// Terrain plan for the new outline: compares today's land with the outline (both north-up 1024 px map masks) and checks
// placed things (world metres, X north, Y east) against it.
public static class GP {
  public static double Min0, Min1, Size; public static int W = 1024;
  // world metres -> north-up map px (the map is welt_klein turned by Rotate270FlipNone: east right, north up)
  public static void ToMap(double xm, double ym, out double mx, out double my){
    double px=(xm*100-Min0)/Size*W-0.5, py=(ym*100-Min1)/Size*W-0.5; mx=py; my=W-1-px; }
  public static void ToWorld(double mx, double my, out double xm, out double ym){
    double px=W-1-my, py=mx; xm=(Min0+(px+0.5)/W*Size)/100; ym=(Min1+(py+0.5)/W*Size)/100; }
  public static double MetresPerPx(){ return Size/W/100; }
  // distance (px) to the nearest pixel where mask==want, -1 beyond lim
  public static float[] Dist(bool[] m,bool want){ var d=new float[W*W]; var q=new Queue<int>();
    for(int i=0;i<d.Length;i++){ if(m[i]==want){ d[i]=0; q.Enqueue(i);} else d[i]=1e9f; }
    while(q.Count>0){ int i=q.Dequeue(); int x=i%W, y=i/W; for(int k=0;k<4;k++){ int nx=x+(k==0?1:k==1?-1:0), ny=y+(k==2?1:k==3?-1:0);
      if(nx<0||ny<0||nx>=W||ny>=W) continue; int j=ny*W+nx; if(d[j]>d[i]+1){ d[j]=d[i]+1; q.Enqueue(j);} } }
    return d; }
  // status of a map point: 0 stays land, 1 becomes sea, 2 sea today and in the outline, 3 new land
  public static int State(bool[] today,bool[] outl,double mx,double my){ int x=(int)Math.Round(mx), y=(int)Math.Round(my);
    if(x<0||y<0||x>=W||y>=W) return 2; int i=y*W+x; return today[i]? (outl[i]?0:1) : (outl[i]?3:2); }
  // nearest outline land that is also land today and at least 'keep' px from the new coast; returns map px or -1
  public static int Nearest(bool[] today,bool[] outl,float[] seaDist,double mx,double my,double keep){
    int best=-1; double bd=1e18; for(int y=0;y<W;y++) for(int x=0;x<W;x++){ int i=y*W+x; if(!today[i]||!outl[i]||seaDist[i]<keep) continue;
      double dd=(x-mx)*(x-mx)+(y-my)*(y-my); if(dd<bd){ bd=dd; best=i; } } return best; }
  public static int[] Count(bool[] today,bool[] outl){ int lost=0,gain=0,stay=0; for(int i=0;i<today.Length;i++){ if(today[i]&&!outl[i]) lost++; else if(!today[i]&&outl[i]) gain++; else if(today[i]) stay++; } return new[]{stay,lost,gain}; }
  public static Bitmap Draw(bool[] today,bool[] outl){ var b=new Bitmap(W,W,PixelFormat.Format32bppArgb);
    var r=b.LockBits(new Rectangle(0,0,W,W),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb); var px=new int[W*W];
    for(int i=0;i<px.Length;i++){ int c; if(today[i]&&outl[i]) c=0x9A9284; else if(today[i]) c=0xD23C30; else if(outl[i]) c=0x4FA048; else c=0x2E4652;
      px[i]=unchecked((int)0xFF000000)|c; }
    System.Runtime.InteropServices.Marshal.Copy(px,0,r.Scan0,px.Length); b.UnlockBits(r); return b; }
}
