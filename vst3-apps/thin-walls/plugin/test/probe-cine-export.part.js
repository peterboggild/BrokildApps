  /* --- 19 cinematic mode and the video export (protocol 7 and 8) --------- */
  /*  Everything here drives the page's own buttons and answers exactly as
      native would; the export is checked on the JPEGs themselves, decoded, not
      on the fact that some string was sent. No backslash may appear in this
      block: it lives inside a template literal. */
  function cineAndExportChecks(done){
    var TWc = window.__TW;
    var bC = document.getElementById("bCine"), qC = document.getElementById("cineQ");
    chk(!!bC && !!qC && !!bC.getAttribute("data-tip") && !!qC.getAttribute("data-tip"),
        "the CINEMATIC toggle and its quality are in the header, both hinted");
    var c0 = TWc.cine();
    chk(c0.on === false && c0.stored === null, "cinematic is off on a fresh profile", JSON.stringify(c0));

    function povHash(){
      var cv = document.getElementById("pov");
      var off = document.createElement("canvas"); off.width = cv.width; off.height = cv.height;
      var c = off.getContext("2d"); c.drawImage(cv, 0, 0);
      var d = c.getImageData(0, 0, off.width, off.height).data, h = 0, n = 0;
      for (var i = 0; i < d.length; i += 4){ h = (h * 31 + d[i] * 3 + d[i+1] * 5 + d[i+2] * 7) % 1000000007; if (d[i] + d[i+1] + d[i+2] > 30) n++; }
      return { h:h, lit:n / (d.length / 4) };
    }
    TWc.render();
    var before = povHash();
    /* (uiprobe.js carries the looped-bump identity check here) */
    bC.click();
    var on = TWc.cine();
    var stOn = null; try { stOn = JSON.parse(on.stored); } catch(e){}
    chk(on.on === true && stOn && stOn.on === true && bC.classList.contains("on"),
        "the toggle turns it on and remembers it", on.stored);
    bC.click();
    var off = TWc.cine();
    var stOff = null; try { stOff = JSON.parse(off.stored); } catch(e){}
    chk(off.on === false && stOff && stOff.on === false && !bC.classList.contains("on"),
        "and off again, remembered as off", off.stored);
    TWc.render();
    var after = povHash();
    chk(before.lit > 0.3 && before.h === after.h,
        "with it off the 3D view is the same picture, pixel for pixel",
        "lit " + (before.lit * 100).toFixed(0) + "%, hash " + before.h + " / " + after.h);

    /* --- recording */
    function sentAfter(mark, kind){ return window.__sent.slice(mark).filter(function (s){ return s.msg && s.msg.k === kind; }); }
    var bR = document.getElementById("bRec"), bE = document.getElementById("bExport");
    window.__fire("rec", { state:"idle", sec:0, max:240, progress:0, file:"", text:"" });
    chk(!!bR && !!bE && bE.disabled && /record a take/i.test(document.getElementById("expNote").textContent),
        "EXPORT is disabled with a reason until there is a take", document.getElementById("expNote").textContent);
    var m0 = window.__sent.length;
    bR.click();
    chk(sentAfter(m0, "recStart").length === 1, "RECORD sends recStart");
    window.__fire("rec", { state:"recording", sec:1.25, max:240, progress:0 });
    setTimeout(function (){
      var cams = sentAfter(m0, "recCam");
      var last = cams.length ? cams[cams.length - 1].msg : null;
      chk(cams.length >= 3 && last && last.t === 1.25 && typeof last.pitch === "number" && typeof last.fov === "number",
          "while recording the page streams recCam with the latest rec.sec", cams.length + " sent, last " + JSON.stringify(last));
      chk(/1.2/.test(document.getElementById("recTime").textContent) && bR.classList.contains("on"),
          "the running time shows and RECORD reads as on", document.getElementById("recTime").textContent);
      var m1 = window.__sent.length;
      bR.click();
      chk(sentAfter(m1, "recStop").length === 1, "pressing it again sends recStop");
      window.__fire("rec", { state:"ready", sec:3.2, max:240, progress:0 });
      var n1 = sentAfter(m0, "recCam").length;
      setTimeout(function (){
        chk(sentAfter(m0, "recCam").length === n1, "recCam stops when the take stops", n1 + " -> " + sentAfter(m0, "recCam").length);
        chk(!bE.disabled, "EXPORT is enabled once a take is held");
        exportChecks(done);
      }, 200);
    }, 260);
  }

  function exportChecks(done){
    var TWx = window.__TW;
    TWx.setParam("lisx", 0.25); TWx.setParam("lisy", 0.2778); TWx.setParam("lisyaw", 0.5);
    TWx.setFurn([{ t:"sofa", x:2.2, y:4.4, yaw:180 }]);
    TWx.render();
    var liveState = JSON.stringify(TWx.state()), liveFurn = JSON.stringify(TWx.furn()), livePF = JSON.stringify(TWx.pitchFov);
    var mark = window.__sent.length;
    document.getElementById("bExport").click();
    var begins = window.__sent.slice(mark).filter(function (s){ return s.msg.k === "vidBegin"; });
    chk(begins.length === 1 && begins[0].msg.w === 1280 && begins[0].msg.h === 720 && begins[0].msg.fps === 30,
        "EXPORT sends vidBegin at the chosen size and rate", begins.length ? JSON.stringify(begins[0].msg) : "none");
    var modal = document.getElementById("expModal");
    chk(!!modal && !modal.hidden, "a modal covers the panel while it exports");
    var ids = PARAMS.map(function (p){ return p[0]; });
    var base = PARAMS.map(function (p){ return p[2]; });
    var ix = ids.indexOf("lisx"), iy = ids.indexOf("lisy"), iw = ids.indexOf("lisyaw");
    var frames = [4.5, 6.0, 7.5].map(function (x){
      var r = base.slice(); r[ix] = x / 18; r[iy] = 2.5 / 9; r[iw] = 0; return r;
    });
    window.__fire("vidPlan", { fps:30, n:3, seconds:0.1, w:64, h:36, ids:ids, frames:frames,
      furn:[{ f:0, items:[{ t:"piano", x:10, y:6, yaw:20 }] }],
      cam:[{ t:0, pitch:-5, fov:70 }, { t:0.1, pitch:5, fov:60 }] });
    var r = modal.getBoundingClientRect();
    var hitEl = document.elementFromPoint(r.left + 20, r.top + 20);
    chk(!!hitEl && modal.contains(hitEl), "while exporting a click lands on the modal, not on a control");
    var acked = 0, ahead = false, jpgs = [], t0 = Date.now();
    function poll(){
      var fr = window.__sent.slice(mark).filter(function (s){ return s.msg.k === "vidFrame"; });
      if (fr.length > acked + 1) ahead = true;
      if (fr.length > acked){
        var m = fr[acked].msg;
        jpgs.push(m);
        window.__fire("vidAck", { i:m.i });
        acked++;
      }
      var ends = window.__sent.slice(mark).filter(function (s){ return s.msg.k === "vidEnd"; });
      if (ends.length || Date.now() - t0 > 6000) return finish();
      setTimeout(poll, 15);
    }
    function finish(){
      var slice = window.__sent.slice(mark);
      var fr = slice.filter(function (s){ return s.msg.k === "vidFrame"; });
      var endAt = -1, lastFrameAt = -1;
      slice.forEach(function (s, k){ if (s.msg.k === "vidEnd") endAt = k; if (s.msg.k === "vidFrame") lastFrameAt = k; });
      chk(fr.length === 3 && !ahead && fr.map(function (s){ return s.msg.i; }).join(",") === "0,1,2",
          "exactly three frames, 0 1 2, and never one before the last was acknowledged",
          fr.length + " frames, ahead " + ahead);
      chk(endAt > lastFrameAt && endAt >= 0, "vidEnd follows the last frame");
      var bad = slice.filter(function (s){ return s.msg.k === "p" || s.msg.k === "furn" || s.msg.k === "touch"; });
      chk(bad.length === 0, "the export sends no p, furn or touch - the live instrument is left alone",
          bad.length ? JSON.stringify(bad[0].msg) : "0");
      chk(jpgs.every(function (m){ return typeof m.jpg === "string" && m.jpg.length > 100 && m.jpg.indexOf("data:") !== 0; }),
          "each frame is base64 JPEG without a data: prefix");
      chk(JSON.stringify(TWx.state()) === liveState && JSON.stringify(TWx.furn()) === liveFurn &&
          JSON.stringify(TWx.pitchFov) === livePF && !TWx.exporting && modal.hidden,
          "afterwards the live values, furniture and camera are exactly as they were",
          "lisx " + TWx.state().lisx + ", furn " + TWx.furn().length);
      var imgs = [], left = jpgs.length;
      jpgs.forEach(function (m, k){
        var im = new Image();
        im.onload = im.onerror = function (){ imgs[k] = im; if (--left === 0) decoded(); };
        im.src = "data:image/jpeg;base64," + m.jpg;
      });
      if (!jpgs.length) decoded();
      function px(im){
        var c = document.createElement("canvas"); c.width = 64; c.height = 36;
        var g = c.getContext("2d"); g.drawImage(im, 0, 0);
        return g.getImageData(0, 0, 64, 36).data;
      }
      function decoded(){
        var sizes = imgs.map(function (im){ return im ? im.naturalWidth + "x" + im.naturalHeight : "none"; });
        chk(imgs.length === 3 && sizes.every(function (s){ return s === "64x36"; }),
            "every frame decodes as a JPEG of exactly the planned size", sizes.join(" "));
        if (imgs.length === 3 && sizes[0] === "64x36"){
          var d0 = px(imgs[0]), d2 = px(imgs[2]);
          var s = 0, s2 = 0, n = 0, diff = 0;
          for (var i = 0; i < d0.length; i += 4){
            var L = (d0[i] + d0[i+1] + d0[i+2]) / 3; s += L; s2 += L * L; n++;
            diff += Math.abs(d0[i] - d2[i]) + Math.abs(d0[i+1] - d2[i+1]) + Math.abs(d0[i+2] - d2[i+2]);
          }
          var mean = s / n, sd = Math.sqrt(Math.max(0, s2 / n - mean * mean));
          chk(sd > 6 && mean > 8, "the first frame is a real picture, not a flat fill", "mean " + mean.toFixed(1) + ", sd " + sd.toFixed(1));
          chk(diff / (n * 3) > 4, "frame 0 at 4.5 m and frame 2 at 7.5 m are different pictures",
              "mean abs difference " + (diff / (n * 3)).toFixed(1));
        }
        TWx.render();
        done();
      }
    }
    poll();
  }

