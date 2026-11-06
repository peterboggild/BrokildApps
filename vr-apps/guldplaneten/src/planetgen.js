/* ===================================================================
 * planetgen.js — a seeded world, defined as a height field ON A SPHERE.
 *
 * Kept as its own file in the repo and INLINED into the published
 * single page: the browser should not have to fetch a second file and
 * the artifact has to stay self-contained.
 *
 * WHY A HEIGHT FIELD AND NOT A SOLID FRACTAL
 * A mandelbulb-style body gives you detail, but it gives you an
 * IMPLICIT surface: placing a house or an alien means ray-marching to
 * find a point and a normal, and you get overhangs and caves, which
 * break the one assumption that makes cheap collision, walking,
 * chunking and landing possible - that the ground is a height above a
 * sphere. On a Quest 2 that is the whole frame budget.
 *
 * h = F(unit vector), built from fBm and ridged noise, is fractal in
 * the sense that matters: infinite detail by adding octaves, and the
 * same function answers at any zoom. A building goes at a lat/long;
 * you ask for the height and the normal; done.
 *
 * And the useful consequence: THE MAP AND THE GROUND ARE ONE FUNCTION.
 * The globe you spin in the cockpit is not a picture of the planet, it
 * is drawn from F(), so the pin you drop lands you in exactly the
 * terrain the map showed you.
 * =================================================================== */

/* ---- 3D value noise on the unit sphere -------------------------- */
function pgHash3(i,j,k){
  let h=Math.imul(i,374761393)^Math.imul(j,668265263)^Math.imul(k,2147483647);
  h=(h^(h>>>13))|0;h=Math.imul(h,1274126177);
  return ((h^(h>>>16))>>>0)/4294967296;
}
function pgMakeNoise3(seed){
  const s=seed|0;
  const at=(i,j,k)=>pgHash3(i+s*7919,j+s*104729,k+s*1299709);
  return function val(x,y,z){
    const xi=Math.floor(x),yi=Math.floor(y),zi=Math.floor(z);
    const xf=x-xi,yf=y-yi,zf=z-zi;
    const u=xf*xf*(3-2*xf),v=yf*yf*(3-2*yf),w=zf*zf*(3-2*zf);
    const c=(di,dj,dk)=>at(xi+di,yi+dj,zi+dk);
    const x00=c(0,0,0)*(1-u)+c(1,0,0)*u, x10=c(0,1,0)*(1-u)+c(1,1,0)*u;
    const x01=c(0,0,1)*(1-u)+c(1,0,1)*u, x11=c(0,1,1)*(1-u)+c(1,1,1)*u;
    return (x00*(1-v)+x10*v)*(1-w)+(x01*(1-v)+x11*v)*w;
  };
}

/* ---- the kinds of world, and what each one is made of ----------- */
const PG_TYPES=[
  {k:"water", name:"Vandplanet",   sea:0.62, amp:1.00, ridge:0.30, atmo:0.9,
   ground:0x3f7f6a, sky:0x2a6fa8, deep:0x123a5e, shore:0xd6c79a, high:0x7fa07a,
   life:0.85, dansk:"vand"},
  {k:"ice",   name:"Isplanet",     sea:0.40, amp:0.95, ridge:0.55, atmo:0.4,
   ground:0xcfe3ef, sky:0x6f93ad, deep:0x5f86a8, shore:0xeaf4fa, high:0xffffff,
   life:0.35, dansk:"is"},
  {k:"rock",  name:"Klippeplanet", sea:0.00, amp:1.45, ridge:0.85, atmo:0.08,
   ground:0x8a7f72, sky:0x0a0a10, deep:0x3a342e, shore:0x9a8d7c, high:0xd8cfc2,
   life:0.10, dansk:"klippe"},
  {k:"fire",  name:"Ildplanet",    sea:0.34, amp:1.30, ridge:0.75, atmo:0.5,
   ground:0x4a2318, sky:0x5a1a08, deep:0xff5a1e, shore:0x8a3a16, high:0x2a1410,
   life:0.15, dansk:"ild"},
  {k:"desert",name:"Ørkenplanet",  sea:0.10, amp:0.85, ridge:0.45, atmo:0.35,
   ground:0xc9a05a, sky:0xb07a3a, deep:0x6a4a22, shore:0xe0c088, high:0xf0ddb0,
   life:0.25, dansk:"ørken"},
  {k:"mixed", name:"Blandet verden",sea:0.46, amp:1.15, ridge:0.60, atmo:0.8,
   ground:0x6f8f4a, sky:0x3f7fc0, deep:0x16406a, shore:0xd8c58a, high:0xb9b2a0,
   life:0.95, dansk:"blandet"},
  {k:"tech",  name:"Teknoplanet",  sea:0.30, amp:0.70, ridge:0.35, atmo:0.6,
   ground:0x4a5560, sky:0x2a3340, deep:0x12202c, shore:0x6a7a88, high:0x9fb6c4,
   life:0.70, dansk:"tekno"}
];

/* Names a six-year-old can say, assembled from two halves plus a
 * number, so the same seed always gives the same name. */
const PG_N1=["Kep","Vel","Tor","Mira","Lyra","Nor","Sol","Bel","Arn","Fyr",
             "Ask","Rim","Dvar","Hel","Bra","Vind","Sten","Glim"];
const PG_N2=["dia","nia","gard","heim","lund","vik","mar","bo","strand",
             "fjeld","dal","øy","havn","borg"];

function pgRand(seed){
  let t=seed>>>0;
  return function(){
    t+=0x6D2B79F5;
    let r=Math.imul(t^(t>>>15),1|t);
    r^=r+Math.imul(r^(r>>>7),61|r);
    return ((r^(r>>>14))>>>0)/4294967296;
  };
}

/* ---- build one world from one number ---------------------------- */
function makePlanet(seed){
  const rnd=pgRand(seed);
  const type=PG_TYPES[Math.floor(rnd()*PG_TYPES.length)];
  const name=PG_N1[Math.floor(rnd()*PG_N1.length)]+
             PG_N2[Math.floor(rnd()*PG_N2.length)]+"-"+
             (2+Math.floor(rnd()*97));
  const n1=pgMakeNoise3(seed+1), n2=pgMakeNoise3(seed+2), n3=pgMakeNoise3(seed+3);
  /* The three scales every landscape needs: where the continents are,
   * where the ranges run, and the crags along them. */
  const warp=0.45+rnd()*0.5;
  const AMP=460*type.amp;

  function fbm3(fn,x,y,z,oct,f0){
    let s=0,a=.5,f=f0||1;
    for(let i=0;i<oct;i++){s+=a*fn(x*f,y*f,z*f);f*=2.02;a*=.5;}
    return s;
  }
  function ridged3(fn,x,y,z,oct,f0){
    let sum=0,amp=.5,f=f0||1,prev=1;
    for(let i=0;i<oct;i++){
      const n=1-Math.abs(fn(x*f,y*f,z*f)*2-1);
      sum+=n*n*amp*prev;prev=n;f*=2.03;amp*=.5;
    }
    return sum;
  }

  /* HEIGHT, in metres above the mean surface, for a unit vector.
   * Everything else in the world - the map, the ground under your
   * boots, where the sea is, where a house can stand - is this.
   *
   * TWO TIERS, which is just what a fractal is for. The planet-scale
   * octaves decide continents, coasts and ranges, and those are what
   * the map draws. Close up, four more octaves at a thousand times the
   * frequency put hills and crags under your boots. One radian is nine
   * hundred kilometres here, so without them the ground within half a
   * kilometre of the ship moved by two metres - measured - and the
   * whole world was a billiard table. The map stays cheap because it
   * never asks for the detail it could not show anyway. */
  function height(x,y,z,local){
    /* Domain warp first, so coastlines meander instead of looking like
     * contour lines on a balloon. */
    const wx=x+warp*0.6*(fbm3(n3,x,y,z,3,1.7)-0.5);
    const wy=y+warp*0.6*(fbm3(n3,y,z,x,3,1.7)-0.5);
    const wz=z+warp*0.6*(fbm3(n3,z,x,y,3,1.7)-0.5);
    const cont=fbm3(n1,wx,wy,wz,5,1.25)-0.5;          // continents
    const rng =ridged3(n2,wx,wy,wz,5,3.1)-0.35;       // ranges
    const crag=(fbm3(n3,wx,wy,wz,4,11.0)-0.5)*0.25;   // crags
    let h=cont*2.0+rng*type.ridge*1.6+crag;
    if(local){
      /* Metre-scale ground. Rolls where the land is gentle, breaks into
       * ridges where it is already steep, so crags grow on mountains
       * and not in the middle of a plain. */
      /* Three local scales, because two left a gap: measured, the
       * ground varied by 55 m within half a kilometre and by 60 m
       * within three, which is a plain with texture on it rather than
       * country you fly across. `hills` is the missing middle - ranges
       * about nine kilometres apart, the scale you actually navigate
       * by from the air. */
      const hills=(ridged3(n2,x,y,z,4,620)-0.42)*0.70;
      const fine=(fbm3(n2,x,y,z,4,2600)-0.5)*0.34;
      const sharp=(ridged3(n1,x,y,z,3,5400)-0.4)*0.30*type.ridge;
      const steep=Math.min(1,Math.abs(rng)*1.6);
      h+=hills*(0.45+0.55*type.ridge)+fine+sharp*(0.35+0.65*steep);
    }
    /* Below sea level the floor flattens out, the way a sea bed does. */
    const sea=type.sea*0.6-0.30;
    if(h<sea)h=sea-(sea-h)*0.45;
    return h*AMP;
  }

  const seaLevel=(type.sea*0.6-0.30)*AMP;

  /* Colour for a height and a slope, in the same linear space the rest
   * of the renderer uses. */
  function colorAt(h,slope,out){
    const C=(hex,o)=>{o.r=((hex>>16)&255)/255;o.g=((hex>>8)&255)/255;o.b=(hex&255)/255;};
    const tmp={r:0,g:0,b:0}, tmp2={r:0,g:0,b:0};
    const mix=(a,b,t,o)=>{o.r=a.r+(b.r-a.r)*t;o.g=a.g+(b.g-a.g)*t;o.b=a.b+(b.b-a.b)*t;};
    if(type.sea>0.02&&h<seaLevel){
      C(type.deep,tmp);C(type.ground,tmp2);
      const d=THREE.MathUtils.clamp((seaLevel-h)/(AMP*0.35),0,1);
      mix(tmp2,tmp,d,out);return out;
    }
    const a=THREE.MathUtils.clamp((h-seaLevel)/(AMP*0.14),0,1);   // shore
    const b=THREE.MathUtils.clamp((h-seaLevel)/(AMP*0.95),0,1);   // highland
    C(type.shore,tmp);C(type.ground,tmp2);
    mix(tmp,tmp2,a,out);
    C(type.high,tmp);
    mix(out,tmp,b*b,out);
    /* Steep faces show bare rock whatever else is going on. */
    const st=THREE.MathUtils.clamp(slope*1.6,0,1)*0.45;
    out.r=out.r*(1-st)+0.42*st;out.g=out.g*(1-st)+0.38*st;out.b=out.b*(1-st)+0.34*st;
    return out;
  }

  return {
    seed:seed, type:type, key:type.k, name:name,
    radius:900, grav:3.0+type.amp*2.2, atmo:type.atmo,
    sky:type.sky, ground:type.ground, seaLevel:seaLevel, amp:AMP,
    height:height, colorAt:colorAt,
    /* Does anything live here? Decided once, by the seed, so the
     * answer is the same every time you visit. */
    hasLife:(rnd()<type.life)
  };
}
