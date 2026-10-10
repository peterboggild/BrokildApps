#include "Engine.h"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace mt;
int main(){ // mtprobe: prints what the engine holds; asserts nothing
  Params p; p.mix=1; p.feedback=0; p.h2On=0; p.h3On=0;
  Engine e; e.setParams(p); e.prepare(48000,256);
  std::vector<float> x(256), L(256), R(256);
  size_t k=0;
  for(int b=0;b<375;b++){ for(int i=0;i<256;i++,k++) x[i]=0.178f*std::sin(2*M_PI*1000*k/48000.0); e.process(x.data(),x.data(),L.data(),R.data(),256); e.service(); }
  float v[66]; e.tapeView(v,66,-6,27);
  for(int i=0;i<66;i++) std::printf("%5.2f:%.2f ", -6+(i+0.5)*0.5, v[i]);
  std::printf("\nT=%.0f N=%lld\n", e.odometerCells(), (long long)e.ringCells());
}
