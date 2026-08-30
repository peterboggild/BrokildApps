/*  PROXIMA CENTAURI b — the survey plate, turnable.
    ==========================================================================

    The plate was always computed rather than photographed, so making it turn
    adds no fakery: the surface, the light, the terminator and the limb all
    follow the sphere's own coordinates and simply move with it.

    The one thing that has to be right, and that a normal planet renderer gets
    backwards: THE WORLD IS TIDALLY LOCKED. The lit face is a fixed
    GEOGRAPHIC region, not a fixed screen region. So the lighting is evaluated
    in the planet's own frame — turn the globe and the day side travels with
    the terrain, the night side is genuinely dark, and the terminator stays
    where it is on the ground. Anything else is a ball with a lamp on it.

    Falls back to the static plate if WebGL is unavailable or the reader has
    asked for reduced motion. */
(function () {
  "use strict";

  var host = document.getElementById("globe");
  if (!host) return;

  var reduce = window.matchMedia &&
               window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  var cv = document.createElement("canvas");
  var gl = null;
  try {
    gl = cv.getContext("webgl", { antialias: false, alpha: true, premultipliedAlpha: false });
  } catch (e) { gl = null; }
  if (!gl || reduce) return;                   // the <img> fallback stays

  /* ---- the sites, as geography ---------------------------------------- */
  /*  Latitude/longitude in the planet's own frame. Longitude 0 is the
      substellar point — the place the star is always overhead. */
  var SITES = [
    { lon: -34.7417, lat: 41.1311, id: "B2311.22", name: "KELL RILLE",
      note: "day 81 · dry cistern" },
    { lon: 89.3183, lat: -17.7031, id: "B2311.67", name: "SABIK TERMINATOR",
      note: "day 204 · vault field" }
  ];

  /* ---- shaders --------------------------------------------------------- */
  var VS =
    "attribute vec2 p;varying vec2 uv;" +
    "void main(){uv=p;gl_Position=vec4(p,0.0,1.0);}";

  var FS = [
    "precision highp float;",
    "varying vec2 uv;",
    "uniform vec2 res;",
    "uniform mat3 rot;",         // view -> planet frame
    "uniform float t;",
    "uniform float rad;",

    /* value noise in three dimensions, so the sphere has no seam and no pole */
    "float h(vec3 q){return fract(sin(dot(q,vec3(127.1,311.7,74.7)))*43758.5453);}",
    "float n3(vec3 q){",
    "  vec3 i=floor(q),f=fract(q);f=f*f*(3.0-2.0*f);",
    "  float a=mix(mix(mix(h(i+vec3(0,0,0)),h(i+vec3(1,0,0)),f.x),",
    "                  mix(h(i+vec3(0,1,0)),h(i+vec3(1,1,0)),f.x),f.y),",
    "              mix(mix(h(i+vec3(0,0,1)),h(i+vec3(1,0,1)),f.x),",
    "                  mix(h(i+vec3(0,1,1)),h(i+vec3(1,1,1)),f.x),f.y),f.z);",
    "  return a;}",
    "float fbm(vec3 q){float a=0.0,w=0.5;for(int i=0;i<6;i++){a+=n3(q)*w;q*=2.03;w*=0.5;}return a;}",

    "void main(){",
    "  vec2 s=(gl_FragCoord.xy-0.5*res)/rad;",
    "  float d2=dot(s,s);",
    "",
    "  if(d2>1.0){",
    /*  THE SKY. A pinhole in the same frame as everything else, so the",
        planet's disc occults it for free: we simply return before drawing",
        anything the sphere covers. */
    "    vec3 sd=normalize(rot*normalize(vec3(s,-2.2)));",
    "    vec3 col=vec3(0.0);",
    "",
    "    vec3 gi=floor(sd*250.0), gf=fract(sd*250.0);",
    "    vec3 rp=vec3(h(gi),h(gi+7.0),h(gi+13.0));",
    "    float pick=h(gi+31.0);",
    "    if(pick>0.9855){",
    "      float dd=length(gf-rp);",
    "      float mag=0.30+0.70*h(gi+53.0);",
    "      float pt=smoothstep(0.30*mag,0.0,dd)*mag;",
    "      float warm=h(gi+71.0);",
    "      col+=pt*mix(vec3(0.72,0.80,0.95),vec3(1.0,0.86,0.72),warm)*0.95;",
    "    }",
    "",
    /*  the star: fixed over longitude 0 because the world is locked to it,",
        so it is behind the observer on the day side and eclipsed on the",
        night side, and only stands clear beside the crescent. */
    "    float ca=clamp(dot(sd,vec3(0.0,0.0,1.0)),-1.0,1.0);",
    "    float ang=acos(ca);",
    "    float disc=smoothstep(0.0150,0.0132,ang);",
    "    float halo=exp(-ang*ang/0.0022)*0.55+exp(-ang*ang/0.045)*0.16;",
    "    col+=vec3(1.00,0.52,0.30)*halo;",
    "    col+=vec3(1.00,0.86,0.74)*disc;",
    "    gl_FragColor=vec4(col,1.0);return;",
    "  }",
    "  float z=sqrt(1.0-d2);",
    "  vec3 nv=vec3(s,z);",                    // normal, view frame
    "  vec3 ng=rot*nv;",                       // normal, planet frame

    /*  the star sits over longitude 0 on the equator and never moves,
        because the world is locked to it */
    "  vec3 L=vec3(0.0,0.0,1.0);",
    "  float nd=dot(ng,L);",

    "  float tex=fbm(ng*4.6+vec3(3.0));",
    "  float fine=fbm(ng*15.0+vec3(11.0));",
    "  float ridge=abs(fbm(ng*8.2+vec3(23.0))-0.5)*2.0;",
    "  float dust=0.35+0.5*fine;",

    "  float lit=pow(max(nd,0.0),0.82);",
    "  float basalt=15.0+tex*20.0+fine*9.0;",
    "  vec3 c;",
    "  float relief=0.62+0.86*tex-0.30*ridge+0.22*fine;",
    "  c.r=basalt*(0.55+0.45*dust)*0.14 + lit*186.0*relief*(0.82+0.30*dust);",
    "  c.g=basalt*(0.40+0.28*dust)*0.12 + lit* 84.0*relief*(0.76+0.28*dust);",
    "  c.b=basalt*(0.42+0.16*dust)*0.17 + lit* 47.0*relief*(0.70+0.22*dust);",

    /* the terminator: a scattering ring where the atmosphere is edge-lit */
    "  float term=exp(-pow(nd/0.13,2.0));",
    "  c+=vec3(120.0,54.0,36.0)*term;",
    /* the night keeps only what the atmosphere carries round */
    "  if(nd<0.0){float k=exp(nd*3.4);c+=vec3(17.0,7.0,7.0)*k;}",

    /* graticule, drawn on the ground so it turns with the world */
    "  float la=asin(clamp(ng.y,-1.0,1.0));",
    "  float lo=atan(ng.x,ng.z);",
    "  float fa=fract(la*(9.0/3.14159)), fo=fract(lo*(9.0/3.14159));",
    "  float da=min(fa,1.0-fa), doo=min(fo,1.0-fo);",
    "  float grat=max(smoothstep(0.035,0.0,da),smoothstep(0.035,0.0,doo));",
    /*  the graticule is a chart overlay, not a structure: it must not glow",
        like a cage across the night side */
    "  c+=vec3(96.0,168.0,158.0)*grat*(0.07+0.30*lit)*z;",

    /* limb darkening, and the rim of atmosphere */
    "  c*=pow(z,0.42);",
    "  float rim=exp(-(1.0-d2)*9.0)*0.85;",
    "  c+=vec3(90.0,44.0,40.0)*rim;",

    "  gl_FragColor=vec4(c/255.0,1.0);",
    "}"
  ].join("\n");

  function sh(type, src) {
    var o = gl.createShader(type);
    gl.shaderSource(o, src); gl.compileShader(o);
    if (!gl.getShaderParameter(o, gl.COMPILE_STATUS)) { return null; }
    return o;
  }
  var vs = sh(gl.VERTEX_SHADER, VS), fs = sh(gl.FRAGMENT_SHADER, FS);
  if (!vs || !fs) return;
  var prog = gl.createProgram();
  gl.attachShader(prog, vs); gl.attachShader(prog, fs); gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) return;
  gl.useProgram(prog);

  var buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1,-1, 3,-1, -1,3]), gl.STATIC_DRAW);
  var loc = gl.getAttribLocation(prog, "p");
  gl.enableVertexAttribArray(loc);
  gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  var uRes = gl.getUniformLocation(prog, "res");
  var uRot = gl.getUniformLocation(prog, "rot");
  var uT   = gl.getUniformLocation(prog, "t");
  var uRad = gl.getUniformLocation(prog, "rad");
  gl.clearColor(0, 0, 0, 0);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  /* ---- the frame ------------------------------------------------------- */
  host.classList.add("live");
  cv.className = "globe-cv";
  host.insertBefore(cv, host.firstChild);

  var marks = document.createElement("div");
  marks.className = "globe-marks";
  host.appendChild(marks);
  SITES.forEach(function (st) {
    var el = document.createElement("div");
    el.className = "gm";
    el.innerHTML = '<span class="ret"></span><b>' + st.name + '</b><i>' +
                   st.id + " · " + st.note + "</i>";
    marks.appendChild(el);
    st.el = el;
  });

  var yaw = -1.16, pitch = 0.15, vyaw = 0.0, drag = null, W = 0, H = 0, RAD = 0;

  function resize() {
    var r = host.getBoundingClientRect();
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    W = Math.max(320, r.width); H = Math.max(240, r.height);
    cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr);
    cv.style.width = W + "px"; cv.style.height = H + "px";
    RAD = Math.min(cv.width, cv.height) * 0.40;
    gl.viewport(0, 0, cv.width, cv.height);
  }

  /* view -> planet frame; the inverse is what maps geography back to screen */
  function matrix(y, p) {
    var cy = Math.cos(y), sy = Math.sin(y), cp = Math.cos(p), sp = Math.sin(p);
    //  rotate about X (pitch) then Y (yaw)
    return [
       cy,  sy*sp,  sy*cp,
      0.0,     cp,    -sp,
      -sy,  cy*sp,  cy*cp
    ];
  }

  function place() {
    var cy = Math.cos(-yaw), sy = Math.sin(-yaw);
    var cp = Math.cos(-pitch), sp = Math.sin(-pitch);
    var r = host.getBoundingClientRect();
    var rad = Math.min(r.width, r.height) * 0.40;
    SITES.forEach(function (st) {
      var la = st.lat * Math.PI / 180, lo = st.lon * Math.PI / 180;
      //  geography -> planet-frame unit vector (lon 0 faces the star, +z)
      var gx = Math.cos(la) * Math.sin(lo),
          gy = Math.sin(la),
          gz = Math.cos(la) * Math.cos(lo);
      //  planet frame -> view frame (inverse of the shader's rot)
      var x1 =  cy*gx - sy*gz,
          z1 =  sy*gx + cy*gz;
      var y2 =  cp*gy + sp*z1,
          z2 = -sp*gy + cp*z1;
      var vis = z2 > 0.06;
      st.el.style.display = vis ? "block" : "none";
      if (!vis) return;
      st.el.style.left = (r.width * 0.5 + x1 * rad) + "px";
      st.el.style.top  = (r.height * 0.5 - y2 * rad) + "px";
      st.el.style.opacity = Math.min(1, (z2 - 0.06) * 6).toFixed(2);
    });
  }

  var last = performance.now();
  function frame(now) {
    var dt = Math.min(0.05, (now - last) / 1000); last = now;
    if (!drag) { yaw += (vyaw + 0.035) * dt; vyaw *= Math.pow(0.12, dt); }
    gl.uniform2f(uRes, cv.width, cv.height);
    gl.uniform1f(uT, now * 0.001);
    gl.uniform1f(uRad, RAD);
    gl.uniformMatrix3fv(uRot, false, new Float32Array(matrix(yaw, pitch)));
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    place();
    requestAnimationFrame(frame);
  }

  /* ---- turning it ------------------------------------------------------ */
  function down(e) {
    drag = { x: e.clientX, y: e.clientY, t: performance.now() };
    host.classList.add("turning");
    try { host.setPointerCapture(e.pointerId); } catch (err) {}
  }
  function move(e) {
    if (!drag) return;
    var dx = e.clientX - drag.x, dy = e.clientY - drag.y;
    var now = performance.now(), dt = Math.max(8, now - drag.t) / 1000;
    yaw += dx * 0.0062;
    pitch = Math.max(-0.85, Math.min(0.85, pitch + dy * 0.0045));
    //  a small fast flick must not launch it round the far side
    vyaw = Math.max(-1.1, Math.min(1.1, (dx * 0.0062) / dt));
    drag.x = e.clientX; drag.y = e.clientY; drag.t = now;
  }
  function up() { drag = null; host.classList.remove("turning"); }

  /*  A test hook, deliberately kept: the whole point of this renderer is
      that the lit face is a geographic region, and the only honest way to
      check that is to aim at a known longitude and read the pixels back. */
  host.__setView = function (y, p) { yaw = y; pitch = p; vyaw = 0; };

  host.addEventListener("pointerdown", down);
  window.addEventListener("pointermove", move);
  window.addEventListener("pointerup", up);
  host.addEventListener("pointercancel", up);
  window.addEventListener("resize", resize);

  resize();
  requestAnimationFrame(frame);
})();
