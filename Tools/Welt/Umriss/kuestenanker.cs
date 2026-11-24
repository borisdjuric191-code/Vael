using System; using System.Collections.Generic;
// Coast anchors for the painted map: today's land is bent onto the picture by a thin plate spline through hand-set anchors;
// where a piece of today's coast lands on painted sea, an anchor pulls it onto the nearest painted land.
public static class KA {
  static double K(double r2){ return r2>1e-12 ? 0.5*r2*Math.Log(r2) : 0; }
  public static double[] WX, WY, U, V; static double[] SU, SV;
  public static void Solve(double s){ int c=WX.Length, n=c+3; SU=Run(U,c,n,s); SV=Run(V,c,n,s); }
  static double[] Run(double[] t,int c,int n,double s){ var A=new double[n,n]; var B=new double[n];
    for(int i=0;i<c;i++){ for(int j=0;j<c;j++) A[i,j]=K((WX[i]-WX[j])*(WX[i]-WX[j])+(WY[i]-WY[j])*(WY[i]-WY[j]))+(i==j?s:0);
      A[i,c]=1; A[i,c+1]=WX[i]; A[i,c+2]=WY[i]; A[c,i]=1; A[c+1,i]=WX[i]; A[c+2,i]=WY[i]; B[i]=t[i]; }
    for(int col=0;col<n;col++){ int p=col; for(int r=col+1;r<n;r++) if(Math.Abs(A[r,col])>Math.Abs(A[p,col])) p=r;
      for(int k=0;k<n;k++){ var tmp=A[col,k]; A[col,k]=A[p,k]; A[p,k]=tmp; } var tb=B[col]; B[col]=B[p]; B[p]=tb;
      for(int r=col+1;r<n;r++){ double f=A[r,col]/A[col,col]; if(f==0) continue; for(int k=col;k<n;k++) A[r,k]-=f*A[col,k]; B[r]-=f*B[col]; } }
    for(int r=n-1;r>=0;r--){ double sum=B[r]; for(int k=r+1;k<n;k++) sum-=A[r,k]*B[k]; B[r]=sum/A[r,r]; } return B; }
  public static void Eval(double x,double y,out double u,out double v){ int c=WX.Length; u=SU[c]+SU[c+1]*x+SU[c+2]*y; v=SV[c]+SV[c+1]*x+SV[c+2]*y;
    for(int i=0;i<c;i++){ double k=K((x-WX[i])*(x-WX[i])+(y-WY[i])*(y-WY[i])); u+=SU[i]*k; v+=SV[i]*k; } }
  public static void Add(double x,double y,double u,double v){ WX=Push(WX,x); WY=Push(WY,y); U=Push(U,u); V=Push(V,v); }
  static double[] Push(double[] a,double v){ var b=new double[a.Length+1]; Array.Copy(a,b,a.Length); b[a.Length]=v; return b; }
  // rounds of: solve, find samples on painted sea, add spread anchors pulling them onto painted land; returns the added
  // anchors as [worldX km, worldY km, u, v] and writes the progress into log
  public static List<double[]> Fit(double[] sx,double[] sy,int[] smx,int[] smy,float[] ins,int W,int rounds,double spacing,int maxAuto,List<string> log){
    var added=new List<double[]>(); var rnd=new Random(7);
    for(int round=1;round<=rounds;round++){ Solve(0.02); var bad=new List<int>(); var bu=new List<double>(); var bv=new List<double>();
      for(int i=0;i<sx.Length;i++){ double u,v; Eval(sx[i],sy[i],out u,out v); double pu=u*W, pv=v*W; int ix=Math.Max(0,Math.Min(W-1,(int)pu)), iy=Math.Max(0,Math.Min(W-1,(int)pv));
        if(ins[iy*W+ix]<1){ bad.Add(i); bu.Add(pu); bv.Add(pv); } }
      log.Add(string.Format("round {0}: {1} of {2} samples on painted sea ({3:F1} %)",round,bad.Count,sx.Length,100.0*bad.Count/sx.Length));
      if(bad.Count<0.003*sx.Length||added.Count>=maxAuto) break;
      var order=new int[bad.Count]; for(int i=0;i<order.Length;i++) order[i]=i; for(int i=order.Length-1;i>0;i--){ int j=rnd.Next(i+1); int t=order[i]; order[i]=order[j]; order[j]=t; }
      var taken=new List<int[]>(); int n=0;
      foreach(int o in order){ int i=bad[o]; bool ok=true; foreach(var t in taken){ if((t[0]-smx[i])*(t[0]-smx[i])+(t[1]-smy[i])*(t[1]-smy[i])<spacing*spacing){ ok=false; break; } }
        if(!ok) continue; int q=Nearest(ins,W,bu[o],bv[o],3,90); if(q<0) continue;
        double tu=(q%W+0.5)/W, tv=(q/W+0.5)/W; Add(sx[i],sy[i],tu,tv); added.Add(new[]{sx[i],sy[i],tu,tv}); taken.Add(new[]{smx[i],smy[i]}); n++;
        if(added.Count>=maxAuto) break; }
      log.Add("  + "+n+" coast anchors"); }
    Solve(0.02); return added; }
  // Ring correspondence: every land pixel gets s (position along the ring) and n (signed offset across it). Per s-bin the
  // band of today's land [a,b] is stretched onto the band of painted land [c,d]; a point keeps its relative place across
  // the band, so land lands on land. Returns targets (map px) for the given sample points (map px).
  static double[] R; static double[] Cum;
  static void Proj(double x,double y,out double s,out double n,out int seg){ int m=R.Length/2; double best=1e18; s=0; n=0; seg=0;
    for(int k=0;k<m-1;k++){ double ax=R[2*k], ay=R[2*k+1], vx=R[2*k+2]-ax, vy=R[2*k+3]-ay, l2=vx*vx+vy*vy, l=Math.Sqrt(l2);
      double t=Math.Max(0,Math.Min(1,((x-ax)*vx+(y-ay)*vy)/l2)); double qx=ax+t*vx, qy=ay+t*vy; double d=(x-qx)*(x-qx)+(y-qy)*(y-qy);
      if(d<best){ best=d; s=Cum[k]+t*l; n=((x-ax)*vy-(y-ay)*vx)/l; seg=k; } } }
  static void Point(double s,double n,out double x,out double y){ int m=R.Length/2; int k=0; while(k<m-2&&Cum[k+1]<s) k++;
    double ax=R[2*k], ay=R[2*k+1], vx=R[2*k+2]-ax, vy=R[2*k+3]-ay, l=Math.Sqrt(vx*vx+vy*vy); double t=(s-Cum[k])/l;
    x=ax+t*vx+n*vy/l; y=ay+t*vy-n*vx/l; }
  static double[][] Bands(bool[] m,int W,int bins,double bw){ var lists=new List<double>[bins]; for(int i=0;i<bins;i++) lists[i]=new List<double>();
    for(int y=0;y<W;y+=2) for(int x=0;x<W;x+=2){ if(!m[y*W+x]) continue; double s,n; int seg; Proj(x,y,out s,out n,out seg); int b=Math.Min(bins-1,(int)(s/bw)); lists[b].Add(n); }
    var lo=new double[bins]; var hi=new double[bins]; var has=new bool[bins];
    for(int i=0;i<bins;i++){ var l=lists[i]; if(l.Count<8) continue; l.Sort(); lo[i]=l[(int)(l.Count*0.03)]; hi[i]=l[(int)(l.Count*0.97)]; has[i]=true; }
    // fill gaps and smooth over 5 bins
    var lo2=new double[bins]; var hi2=new double[bins];
    for(int i=0;i<bins;i++){ double a=0,b=0; int c=0; for(int k=-2;k<=2;k++){ int j=i+k; if(j<0||j>=bins||!has[j]) continue; a+=lo[j]; b+=hi[j]; c++; }
      if(c>0){ lo2[i]=a/c; hi2[i]=b/c; } else { lo2[i]=double.NaN; hi2[i]=double.NaN; } }
    return new[]{lo2,hi2}; }
  // only the large land masses (islands and sea stacks would widen the bands into the sea)
  public static bool[] MainLand(bool[] m,int W,int minPx){ var o=new bool[m.Length]; var seen=new bool[m.Length]; var st=new Stack<int>(); var comp=new List<int>();
    for(int s=0;s<m.Length;s++){ if(!m[s]||seen[s]) continue; comp.Clear(); st.Push(s); seen[s]=true;
      while(st.Count>0){ int i=st.Pop(); comp.Add(i); int x=i%W, y=i/W;
        for(int k=0;k<4;k++){ int nx=x+(k==0?1:k==1?-1:0), ny=y+(k==2?1:k==3?-1:0); if(nx<0||ny<0||nx>=W||ny>=W) continue; int j=ny*W+nx; if(m[j]&&!seen[j]){ seen[j]=true; st.Push(j);} } }
      if(comp.Count>=minPx) foreach(int i in comp) o[i]=true; }
    return o; }
  public static double[] RingTargets(double[] ring,bool[] today,bool[] painted,int W,double[] px,double[] py){
    today=MainLand(today,W,3000); painted=MainLand(painted,W,3000);
    R=ring; int m=ring.Length/2; Cum=new double[m]; for(int k=1;k<m;k++){ double dx=ring[2*k]-ring[2*k-2], dy=ring[2*k+1]-ring[2*k-1]; Cum[k]=Cum[k-1]+Math.Sqrt(dx*dx+dy*dy); }
    double bw=8; int bins=(int)(Cum[m-1]/bw)+1; var bt=Bands(today,W,bins,bw); var bp=Bands(painted,W,bins,bw);
    var o=new double[px.Length*2];
    for(int i=0;i<px.Length;i++){ double s,n; int seg; Proj(px[i],py[i],out s,out n,out seg); int b=Math.Min(bins-1,(int)(s/bw));
      double a=bt[0][b], bb=bt[1][b], c=bp[0][b], d=bp[1][b];
      if(double.IsNaN(a)||double.IsNaN(c)||bb-a<2){ o[2*i]=double.NaN; o[2*i+1]=double.NaN; continue; }
      double f=Math.Max(0,Math.Min(1,(n-a)/(bb-a))); double n2=c+f*(d-c); double x,y; Point(s,n2,out x,out y); o[2*i]=x; o[2*i+1]=y; }
    return o; }
  public static int OnSea(float[] ins,int W,double[] sx,double[] sy){ int bad=0; Solve(0.02);
    for(int i=0;i<sx.Length;i++){ double u,v; Eval(sx[i],sy[i],out u,out v); int ix=Math.Max(0,Math.Min(W-1,(int)(u*W))), iy=Math.Max(0,Math.Min(W-1,(int)(v*W))); if(ins[iy*W+ix]<1) bad++; }
    return bad; }
  // distance in px to the nearest painted sea pixel (BFS), so 'inside by d' can be asked
  public static float[] Inside(bool[] land,int W){ var d=new float[W*W]; var q=new Queue<int>();
    for(int i=0;i<d.Length;i++){ if(!land[i]){ d[i]=0; q.Enqueue(i);} else d[i]=1e9f; }
    while(q.Count>0){ int i=q.Dequeue(); int x=i%W, y=i/W; for(int k=0;k<4;k++){ int nx=x+(k==0?1:k==1?-1:0), ny=y+(k==2?1:k==3?-1:0);
      if(nx<0||ny<0||nx>=W||ny>=W) continue; int j=ny*W+nx; if(d[j]>d[i]+1){ d[j]=d[i]+1; q.Enqueue(j);} } } return d; }
  // nearest pixel at least 'inside' px from the sea, within 'lim' px; -1 if none
  public static int Nearest(float[] ins,int W,double px,double py,double inside,int lim){ int best=-1; double bd=1e18;
    int x0=Math.Max(0,(int)px-lim), x1=Math.Min(W-1,(int)px+lim), y0=Math.Max(0,(int)py-lim), y1=Math.Min(W-1,(int)py+lim);
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){ if(ins[y*W+x]<inside) continue; double dd=(x-px)*(x-px)+(y-py)*(y-py); if(dd<bd){ bd=dd; best=y*W+x; } } return best; }
}
