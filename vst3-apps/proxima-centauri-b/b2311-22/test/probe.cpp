#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>
using namespace ab;
static double goertz(const float*x,int from,int to,double f,double fs){
    const double w=2.0*3.14159265358979*f/fs,c=2.0*std::cos(w);
    double s0=0,s1=0,s2=0;
    for(int i=from;i<to;i++){s0=x[i]+c*s1-s2;s2=s1;s1=s0;}
    return std::sqrt(s1*s1+s2*s2-c*s1*s2)/(to-from);
}
static void mk(Engine&e,int self,int other,float conv,float comm,float pl){
    e.p.specimen=(float)self;e.p.other=(float)other;e.p.converse=conv;
    e.p.commune=comm;e.p.plastic=pl;e.p.wake=0;e.p.transit=0;e.prepare(48000.0,512);
}
static void hold(Engine&e,float*L,float*R,int N){
    e.noteOn(57,0.9f);
    int done=0;while(done<N){int m=std::min(256,N-done);e.process(L+done,R+done,m);done+=m;}
}
int main(){
    const double fs=48000.0; const int N=9*48000;
    std::vector<float> l0(N),r0(N),l1(N),r1(N);
    Engine a; mk(a,3,61,0.7f,0.0f,0); hold(a,l0.data(),r0.data(),N);
    Engine b; mk(b,3,61,0.7f,0.9f,0); hold(b,l1.data(),r1.data(),N);
    //  where does the interjection land? print rms per half-second of the difference
    std::printf("half-second rms of (membrane on - off):\n  ");
    for(int s=0;s<18;s++){
        double d=0;int f=(int)(s*0.5*fs),t=(int)((s+1)*0.5*fs);
        for(int i=f;i<t;i++)d+=(double)(l1[i]-l0[i])*(l1[i]-l0[i]);
        std::printf("%.4f ",std::sqrt(d/(t-f)));
    }
    std::printf("\n");
    int F=(int)(5.3*fs),T=(int)(6.8*fs);
    double d=0,t=0,e0=0,e1=0;
    for(int i=F;i<T;i++){d+=std::fabs(l1[i]-l0[i]);t+=std::max(std::fabs(l1[i]),std::fabs(l0[i]));
        e0+=(double)l0[i]*l0[i]; e1+=(double)l1[i]*l1[i];}
    std::printf("window 5.3-6.8s: rel change %.3f | rms off %.4f on %.4f\n",
        t>0?d/t:0.0,std::sqrt(e0/(T-F)),std::sqrt(e1/(T-F)));
    Specimen ss,os; generateSpecimen(3,ss); generateSpecimen(61,os);
    const double f0=440.0*std::pow(2.0,(57.0-69.0)/12.0);
    const double fa=f0*ss.ratio[0], fb=os.voiceHz*os.ratio[0];
    std::vector<float> diff(N); for(int i=0;i<N;i++) diff[i]=l1[i]-l0[i];
    std::printf("fa=%.1f fb=%.1f | SUM %.1f: off %.2e on %.2e | DIFF %.1f: off %.2e on %.2e | ctl off %.2e on %.2e\n",
        fa,fb,fa+fb,goertz(l0.data(),F,T,fa+fb,fs),goertz(l1.data(),F,T,fa+fb,fs),
        std::fabs(fa-fb),goertz(l0.data(),F,T,std::fabs(fa-fb),fs),goertz(l1.data(),F,T,std::fabs(fa-fb),fs),
        goertz(l0.data(),F,T,(fa+fb)*1.0733,fs),goertz(l1.data(),F,T,(fa+fb)*1.0733,fs));
    //  the membrane must VANISH when only one body sounds
    { std::vector<float> a0(N),b0(N),a1(N),b1(N);
      Engine x; mk(x,3,61,0.0f,0.0f,0); hold(x,a0.data(),b0.data(),N);
      Engine y; mk(y,3,61,0.0f,0.9f,0); hold(y,a1.data(),b1.data(),N);
      std::printf("membrane with no partner (converse 0): identical = %s\n",
        std::memcmp(a0.data(),a1.data(),(size_t)N*4)==0?"YES":"NO"); }
    return 0;
}
