  /* --- 20 light sync and wall pictures (protocol 9 and 10) --------------- */
  /*  No backslash may appear in this block: it lives inside a template
      literal. Every claim is checked on pixels or on what went on the wire. */
  function v5Checks(done){
    var T5 = window.__TW;
    var NLc = String.fromCharCode(10);
    function pix(cv){
      var off = document.createElement("canvas"); off.width = cv.width; off.height = cv.height;
      var c = off.getContext("2d"); c.drawImage(cv, 0, 0);
      return c.getImageData(0, 0, off.width, off.height).data;
    }
    function hash(d){ var h = 0; for (var i = 0; i < d.length; i += 4) h = (h * 31 + d[i] * 3 + d[i+1] * 5 + d[i+2] * 7) % 1000000007; return h; }
    function mad(a, b){ var s = 0, n = 0; for (var i = 0; i < a.length; i += 4){ s += Math.abs(a[i]-b[i]) + Math.abs(a[i+1]-b[i+1]) + Math.abs(a[i+2]-b[i+2]); n++; } return s / (n * 3); }
    function scene5(extra){ window.__fire("scene", Object.assign({}, SCENE, extra)); }
    var pov = document.getElementById("pov"), plan = document.getElementById("plan");

    /* ---- the controls */
    var ctl = ["syncS", "syncFollow", "syncBeat", "syncWin"].map(function (id){ return document.getElementById(id); });
    var sy0 = T5.sync();
    chk(ctl.every(function (e){ return e && e.getAttribute("data-tip"); }) && sy0.s === 0.2 && sy0.mode === "follow" && !sy0.win,
        "LIGHT SYNC has a slider, FOLLOW / BEAT and WINDOW, all hinted; LOW 20 percent, FOLLOW, window off by default",
        JSON.stringify({ s:sy0.s, mode:sy0.mode, win:sy0.win }));
    ctl[0].value = "0.55"; ctl[0].dispatchEvent(new Event("input", { bubbles:true }));
    ctl[2].click(); ctl[3].click();
    var st = null; try { st = JSON.parse(T5.sync().stored); } catch(e){}
    chk(st && st.s === 0.55 && st.mode === "beat" && st.win === true, "the three settings are remembered", T5.sync().stored);
    ctl[1].click(); ctl[3].click();

    /* ---- sync 0 is today's picture, and costs no draws */
    T5.setSync({ s:0 });
    scene5({ light:[0.9, 0.9, 0.9], beat:{ bpm:120, ppq:1, playing:false } });
    T5.render();
    var d0 = pix(pov), h0 = hash(d0);
    var fsNow = T5.fsSource(), leg = fsNow;
    ["  vec2 dux=dFdx(vUV), duy=dFdy(vUV);",
     "  if(k==7||k==33) mt.emis*=uLampK[rm];",
     "  if(k==8) mt.emis*=uWinK;"].forEach(function (l){ leg = leg.replace(l + NLc, ""); });
    var i60 = leg.indexOf("  if(k==60){"), e60 = i60 >= 0 ? leg.indexOf(NLc, i60) : -1;
    if (i60 >= 0 && e60 > i60) leg = leg.slice(0, i60) + leg.slice(e60 + 1);
    var sw1 = T5.swapFS(leg); T5.render(); var hL = hash(pix(pov));
    var sw2 = T5.swapFS(null); T5.render(); var hB = hash(pix(pov));
    chk(sw1 === "ok" && sw2 === "ok" && leg.indexOf("uLampK[rm]") < 0 && fsNow.indexOf("uLampK[rm]") > 0 && hL === h0 && hB === h0,
        "at SYNC 0 the view is the picture the shader drew before light sync existed, pixel for pixel",
        sw1 + "/" + sw2 + ", legacy " + hL + ", now " + h0 + ", again " + hB);
    chk(!T5.sync().active && T5.sync().k.join() === "1,1,1", "at SYNC 0 sound in the rooms moves nothing and asks for no frames",
        JSON.stringify(T5.sync().k) + ", active " + T5.sync().active);

    /* ---- FOLLOW: the room with the sound, and only that room */
    T5.setSync({ s:1, mode:"follow", win:false });
    scene5({ light:[0, 0, 0] }); T5.syncSettle(); T5.render(); var dQ = pix(pov);
    scene5({ light:[1, 0, 0] });
    chk(T5.sync().active && T5.pending, "with SYNC up and sound in a room the view keeps drawing", "active " + T5.sync().active + ", pending " + T5.pending);
    T5.syncSettle(); T5.render(); var dA = pix(pov); var kA = T5.sync().k;
    scene5({ light:[0, 1, 1] }); T5.syncSettle(); T5.render(); var dB = pix(pov); var kB = T5.sync().k;
    /* measured on the LARGE pendant's own glowing shade: the light it throws through
       an open door into the next room follows the next room too, as it should */
    var lp = T5.project([2.15, 3.45, 2.8 - 0.555]), rr = pov.getBoundingClientRect();
    function near(a, b){
      if (!lp) return -1;
      var cx = Math.round((lp[0] - rr.left) * pov.width / rr.width), cy = Math.round((lp[1] - rr.top) * pov.height / rr.height), s2 = 0, n2 = 0;
      for (var y = Math.max(0, cy - 8); y < Math.min(pov.height, cy + 8); y++) for (var x = Math.max(0, cx - 8); x < Math.min(pov.width, cx + 8); x++){
        var i = (y * pov.width + x) * 4; s2 += Math.abs(a[i]-b[i]) + Math.abs(a[i+1]-b[i+1]) + Math.abs(a[i+2]-b[i+2]); n2++; }
      return s2 / (n2 * 3);
    }
    var mA = near(dQ, dA), mB = near(dQ, dB);
    chk(kA[0] > 1.8 && kA[1] < 0.6 && kA[2] < 0.6 && kB[0] < 0.6 && mA > 6 && mA > mB * 3,
        "sound in the LARGE room lifts the LARGE pendant you are looking at; sound in the other two rooms leaves it alone",
        "k " + kA.map(function (v){ return v.toFixed(2); }).join("/") + ", LARGE loud " + mA.toFixed(1) + ", others loud " + mB.toFixed(1));

    /* ---- BEAT: pulses on the clock, and follows the sound when stopped */
    T5.setSync({ s:1, mode:"beat" });
    scene5({ light:[0, 0, 0], beat:{ bpm:120, ppq:8.0, playing:true } });
    T5.syncSettle(); var kb0 = T5.sync().k[0];
    setTimeout(function (){
      T5.syncSettle(); var kb1 = T5.sync().k[0];
      chk(T5.sync().active && kb0 > 1.7 && kb1 < kb0 - 0.6, "BEAT pulses: bright on the beat, falling away after it, in silence",
          "on the beat " + kb0.toFixed(2) + ", 0.15 s later " + kb1.toFixed(2));
      scene5({ light:[0, 0, 0], beat:{ bpm:120, ppq:8.3, playing:false } });
      T5.syncSettle();
      chk(Math.abs(T5.sync().k[0] - 0.5) < 0.01, "with the transport stopped BEAT falls back to FOLLOW", T5.sync().k[0].toFixed(3));
      T5.setSync({ s:1, mode:"follow" });
      exportLight();
    }, 150);

    /* ---- an export uses the take's light, never the live one */
    function exportLight(){
      window.__fire("rec", { state:"ready", sec:0.1, max:240, progress:0 });
      var mark = window.__sent.length;
      document.getElementById("bExport").click();
      var ids = PARAMS.map(function (p){ return p[0]; }), base = PARAMS.map(function (p){ return p[2]; });
      var frames = [base.slice(), base.slice(), base.slice()];
      scene5({ light:[1, 1, 1] });                       /* live: loud everywhere */
      window.__fire("vidPlan", { fps:30, n:3, seconds:0.1, w:96, h:54, ids:ids, frames:frames, furn:[], cam:[],
        light:[[1, 0, 0], [0, 0, 0], [1, 0, 0]], beat:[[0, 0, 0], [0, 0, 0], [0, 0, 0]] });
      var acked = 0, jp = [], t0 = Date.now();
      (function poll(){
        var fr = window.__sent.slice(mark).filter(function (s){ return s.msg.k === "vidFrame"; });
        if (fr.length > acked){ jp.push(fr[acked].msg.jpg); window.__fire("vidAck", { i:fr[acked].msg.i }); acked++; }
        var end = window.__sent.slice(mark).some(function (s){ return s.msg.k === "vidEnd"; });
        if (!end && Date.now() - t0 < 5000){ setTimeout(poll, 15); return; }
        window.__fire("rec", { state:"done", sec:0.1, max:240, progress:1, file:"x.mp4" });
        var ims = [], left = jp.length;
        jp.forEach(function (j, k){ var im = new Image(); im.onload = im.onerror = function (){ ims[k] = im; if (--left === 0) cmp(); }; im.src = "data:image/jpeg;base64," + j; });
        if (!jp.length) cmp();
        function px2(im){ var c = document.createElement("canvas"); c.width = 96; c.height = 54; var g = c.getContext("2d"); g.drawImage(im, 0, 0); return g.getImageData(0, 0, 96, 54).data; }
        function cmp(){
          var ok = ims.length === 3 && ims.every(function (im){ return im && im.naturalWidth === 96; });
          var a = ok ? mad(px2(ims[0]), px2(ims[1])) : -1, b = ok ? mad(px2(ims[0]), px2(ims[2])) : -1;
          chk(ok && a > 2 && a > b * 2, "exported frames follow the take's own light: the quiet frame is darker, the loud ones match, whatever the live rooms do",
              "loud vs quiet " + a.toFixed(1) + ", loud vs loud " + b.toFixed(1));
          pictures();
        }
      })();
    }

    /* ---- pictures */
    function pictures(){
      T5.setSync({ s:0 });
      scene5({ light:[0, 0, 0] }); T5.syncSettle();
      T5.setParam("lisx", 0.25); T5.setParam("lisy", 0.2778); T5.setParam("lisyaw", 0.5);
      T5.render();
      var before = pix(pov);
      var cv = document.createElement("canvas"); cv.width = 400; cv.height = 300;
      var g = cv.getContext("2d");
      g.fillStyle = "#ff00ff"; g.fillRect(0, 0, 200, 300); g.fillStyle = "#00ff00"; g.fillRect(200, 0, 200, 300);
      cv.toBlob(function (blob){
        var inp = document.getElementById("picFile");
        var dt = new DataTransfer(); dt.items.add(new File([blob], "test.png", { type:"image/png" }));
        var mark = window.__sent.length;
        inp.files = dt.files;
        inp.dispatchEvent(new Event("change", { bubbles:true }));
        var t0 = Date.now();
        (function wait(){
          if (!T5.picArmed && Date.now() - t0 < 3000){ setTimeout(wait, 20); return; }
          var adds = window.__sent.slice(mark).filter(function (s){ return s.msg.k === "picAdd"; });
          var a0 = adds[0] && adds[0].msg;
          chk(adds.length === 1 && a0 && a0.id === T5.picArmed && typeof a0.jpg === "string" && a0.jpg.length > 200 && a0.jpg.indexOf("data:") !== 0,
              "LOAD shrinks the file and sends picAdd once, then waits for a wall", adds.length + " picAdd, armed " + T5.picArmed);
          var jpgKeep = a0 ? a0.jpg : "", idKeep = a0 ? a0.id : "";
          /* hang it on the LARGE west wall, clicked in the plan */
          var PFp = T5.planFit(), pr = plan.getBoundingClientRect();
          var cx = pr.left + PFp.ox + 0.12 * PFp.s, cy = pr.top + PFp.oy + (9 - 2.5) * PFp.s;
          var m2 = window.__sent.length;
          plan.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, clientX:cx, clientY:cy, button:0, pointerId:7 }));
          window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, clientX:cx, clientY:cy, pointerId:7 }));
          var p1 = T5.pics();
          var sentP = window.__sent.slice(m2).filter(function (s){ return s.msg.k === "pics"; });
          chk(p1.length === 1 && p1[0].room === 0 && p1[0].wall === 0 && Math.abs(p1[0].along - 2.5) < 0.2 && !T5.picArmed &&
              sentP.length >= 1 && sentP[sentP.length - 1].msg.items.length === 1,
              "a click near a wall in the plan hangs it there and sends the layout",
              JSON.stringify(p1[0] || null));
          setTimeout(function (){
            T5.render();
            var after = pix(pov);
            var pp = T5.picPlace(0), sc = T5.project([pp.c[0] + pp.n[0] * 0.03, pp.c[1] + pp.n[1] * 0.03, pp.zc]);
            var r = pov.getBoundingClientRect(), kx = pov.width / r.width, ky = pov.height / r.height;
            function count(d, px, py, rad){
              var mg = 0, gr = 0;
              for (var y = Math.max(0, py - rad); y < Math.min(pov.height, py + rad); y++)
                for (var x = Math.max(0, px - rad); x < Math.min(pov.width, px + rad); x++){
                  var i = (y * pov.width + x) * 4, R = d[i], G = d[i+1], B = d[i+2];
                  if (R > G + 25 && B > G + 15) mg++;
                  if (G > R + 20 && G > B + 10) gr++;
                }
              return [mg, gr];
            }
            var px = sc ? Math.round((sc[0] - r.left) * kx) : 0, py = sc ? Math.round((sc[1] - r.top) * ky) : 0;
            var cA = count(after, px, py, 60), cB = count(before, px, py, 60);
            chk(sc && cA[0] > 40 && cA[1] > 40 && cB[0] + cB[1] < 5,
                "the picture's own pixels, magenta and green, appear on the wall in the 3D view",
                "magenta " + cA[0] + ", green " + cA[1] + " (before " + (cB[0] + cB[1]) + ")");
            var pd = pix(plan), pmg = 0, pgr = 0;
            var ppx = Math.round((PFp.ox + 0.05 * PFp.s) * plan.width / pr.width), ppy = Math.round((PFp.oy + 6.5 * PFp.s) * plan.height / pr.height);
            var rad = Math.round(0.8 * PFp.s * plan.width / pr.width);
            for (var y = Math.max(0, ppy - rad); y < ppy + rad; y++) for (var x = Math.max(0, ppx - 12); x < ppx + 12; x++){
              var i = (y * plan.width + x) * 4;
              if (pd[i] > pd[i+1] + 40 && pd[i+2] > pd[i+1] + 40) pmg++;
              if (pd[i+1] > pd[i] + 40 && pd[i+1] > pd[i+2] + 40) pgr++;
            }
            chk(pmg > 3 && pgr > 3 && T5.planPics().length === 1, "and on its wall in the plan, as a strip of the picture itself", "magenta " + pmg + ", green " + pgr);

            /* move it along in the plan */
            var a0v = T5.pics()[0].along, m3 = window.__sent.length;
            var sx = pr.left + PFp.ox + 0.05 * PFp.s, sy = pr.top + PFp.oy + (9 - a0v) * PFp.s;
            plan.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, clientX:sx, clientY:sy, button:0, pointerId:8 }));
            window.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, clientX:sx, clientY:sy - 0.6 * PFp.s, pointerId:8 }));
            window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, clientX:sx, clientY:sy - 0.6 * PFp.s, pointerId:8 }));
            var a1v = T5.pics()[0].along;
            var sent3 = window.__sent.slice(m3).filter(function (s){ return s.msg.k === "pics"; });
            chk(T5.picSel === 0 && Math.abs(a1v - a0v - 0.6) < 0.05 && sent3.length >= 1 &&
                Math.abs(sent3[sent3.length - 1].msg.items[0].along - a1v) < 0.002,
                "dragging it in the plan slides it along the wall, and the final position is sent", a0v.toFixed(2) + " -> " + a1v.toFixed(2));
            /* and up in the 3D view */
            T5.render();
            var pp2 = T5.picPlace(0), sc2 = T5.project([pp2.c[0] + pp2.n[0] * 0.03, pp2.c[1] + pp2.n[1] * 0.03, pp2.zc]);
            var z0 = T5.pics()[0].z;
            pov.dispatchEvent(new PointerEvent("pointerdown", { bubbles:true, clientX:sc2[0], clientY:sc2[1], button:0, pointerId:9 }));
            window.dispatchEvent(new PointerEvent("pointermove", { bubbles:true, clientX:sc2[0], clientY:sc2[1] - 30, pointerId:9 }));
            window.dispatchEvent(new PointerEvent("pointerup", { bubbles:true, clientX:sc2[0], clientY:sc2[1] - 30, pointerId:9 }));
            chk(T5.pics()[0].z > z0 + 0.1, "dragging it in the 3D view lifts it up the wall", z0.toFixed(2) + " -> " + T5.pics()[0].z.toFixed(2));
            /* size, frame, kind */
            document.querySelector('[data-picsize="0.4"]').click();
            var wS = T5.pics()[0].w;
            var sw = document.getElementById("picW"); sw.value = "1.0"; sw.dispatchEvent(new Event("input", { bubbles:true }));
            var wW = T5.pics()[0].w;
            var sf = document.getElementById("picFrame"); sf.value = "2"; sf.dispatchEvent(new Event("change", { bubbles:true }));
            var sk = document.getElementById("picKind"); sk.value = "1"; sk.dispatchEvent(new Event("change", { bubbles:true }));
            var q = T5.pics()[0];
            chk(Math.abs(wS - 0.4) < 1e-6 && Math.abs(wW - 1.0) < 1e-6 && q.frame === 2 && q.kind === 1 && Math.abs(q.aspect - 0.75) < 0.02,
                "S sets 40 cm, the slider sets 1 m, FRAME and KIND change it, and the aspect stays the image's",
                "w " + wS + " / " + wW + ", frame " + q.frame + ", kind " + q.kind + ", aspect " + q.aspect.toFixed(3));
            chk(/absorb/i.test(document.getElementById("picKind").getAttribute("data-tip")) &&
                /nothing you hear/i.test(document.getElementById("picKind").getAttribute("data-tip")),
                "the KIND hint says honestly that a print does nothing audible and a panel absorbs");
            /* a door is never covered */
            T5.setPics([{ id:idKeep, room:0, wall:1, along:2.5, z:1.2, w:0.7, aspect:0.75, frame:0, kind:0 }], null);
            var qd = T5.pics()[0], hgt = qd.w * qd.aspect;
            var clear = qd.along + qd.w / 2 <= 2.05 - 0.11 || qd.along - qd.w / 2 >= 2.95 + 0.11 || qd.z - hgt / 2 > 2.05 + 0.1;
            chk(clear, "a picture asked to hang over a doorway is moved clear of it", "along " + qd.along.toFixed(2) + ", z " + qd.z.toFixed(2));
            /* delete */
            T5.selectPic(0);
            var m4 = window.__sent.length;
            document.getElementById("picDel").click();
            var s4 = window.__sent.slice(m4).filter(function (s){ return s.msg.k === "pics"; });
            chk(T5.pics().length === 0 && s4.length === 1 && s4[0].msg.items.length === 0, "DELETE takes it down and sends the empty layout");
            /* restored from native */
            window.__fire("pics", { items:[{ id:"q1", room:0, wall:0, along:2.5, z:1.5, w:0.8, aspect:0.75, frame:3, kind:0 }], images:{ q1:jpgKeep } });
            var t1 = Date.now();
            (function wait2(){
              if (!T5.picReady("q1") && Date.now() - t1 < 3000){ setTimeout(wait2, 20); return; }
              T5.render();
              var dR = pix(pov);
              var pp3 = T5.picPlace(0), sc3 = T5.project([pp3.c[0] + pp3.n[0] * 0.04, pp3.c[1] + pp3.n[1] * 0.04, pp3.zc]);
              var cR = sc3 ? count(dR, Math.round((sc3[0] - r.left) * kx), Math.round((sc3[1] - r.top) * ky), 60) : [0, 0];
              chk(T5.pics().length === 1 && T5.picReady("q1") && cR[0] > 40 && cR[1] > 40,
                  "a pics event with its images puts the picture back, pixels and all", "magenta " + cR[0] + ", green " + cR[1]);
              T5.setPics([], null);
              T5.setSync({ s:0.2, mode:"follow", win:false });
              scene5({ light:[0, 0, 0] }); T5.syncSettle();
              T5.render();
              done();
            })();
          }, 120);
        })();
      }, "image/png");
    }
  }

