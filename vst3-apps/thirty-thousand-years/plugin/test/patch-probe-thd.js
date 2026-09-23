/*  A probe mode that measures the distortion a stratum adds when it should be
    adding none. Peter heard clipping on presets whose output is under 0 dB,
    and the stage meters put STRUCTURE at 1.4-1.9 on its own bus - so the
    question is how hard the internal saturators are working at ordinary
    settings. This answers it in dB rather than by reading the code.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/test/probe.cpp";
let s = fs.readFileSync(p, "utf8");
const a = `    return 0;
}`;
const b = `    else if (what == "thd")
    {
        /*  A sine through MASS with the filter wide open and no drive: every
            harmonic here is distortion this instrument invented. */
        struct Case { const char* name; float o1; float o2; int fmode; float cut; float drive; float sub; };
        Case cases[] = {
            { "one sine, SVF wide open",          0.8f, 0.0f, 1, 1.0f, 0.0f, 0.0f },
            { "one sine, LADDER wide open",       0.8f, 0.0f, 0, 1.0f, 0.0f, 0.0f },
            { "one sine at FULL level, LADDER",   1.0f, 0.0f, 0, 1.0f, 0.0f, 0.0f },
            { "two sines at full, LADDER",        1.0f, 1.0f, 0, 1.0f, 0.0f, 0.0f },
            { "two sines + sub at full, LADDER",  1.0f, 1.0f, 0, 1.0f, 0.0f, 1.0f },
            { "two sines at full, DRIVE 100 %",   1.0f, 1.0f, 0, 1.0f, 1.0f, 0.0f },
        };
        std::printf ("MASS: a sine in, harmonics out (the instrument's own distortion)\\n");
        for (const Case& c : cases)
        {
            Engine e; bareSine (e); e.prepare (48000.0, 512); bareSine (e);
            e.p[P_m_o1wave] = 0; e.p[P_m_o2wave] = 0;
            e.p[P_m_o1lvl] = c.o1; e.p[P_m_o2lvl] = c.o2; e.p[P_m_fmode] = (float) c.fmode;
            e.p[P_m_cut] = c.cut; e.p[P_m_fdrive] = c.drive; e.p[P_m_res] = 0.0f;
            e.p[P_m_sub] = c.sub > 0 ? 1.0f : 0.0f; e.p[P_m_sublvl] = c.sub;
            e.p[P_m_o2fine] = 0.5f; e.p[P_m_o2semi] = 0.5f;   // both oscillators on the SAME note
            e.noteOn (45, 1.0f); render (e, 1.0); Take t = render (e, 1.0);
            const double f0 = 110.0;
            double fund = goertzel (t.L, 48000, f0, 0, t.n()), harm = 0;
            for (int k = 2; k <= 12; ++k) harm += goertzel (t.L, 48000, f0 * k, 0, t.n());
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            std::printf ("  %-34s THD %6.1f dB   bus peak %.2f   out peak %.2f\\n",
                         c.name, 10 * std::log10 ((harm + 1e-20) / (fund + 1e-20)), e.stagePeak[Engine::ST_MASS], pk);
        }

        /*  STRUCTURE: a bowed body at ordinary settings, and how much its own
            output saturator is taking off. */
        std::printf ("\\nSTRUCTURE: a bowed plate, and what its output saturator does\\n");
        for (float ex : { 0.3f, 0.5f, 0.7f, 1.0f })
        {
            Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_st_on] = 1; e.p[P_st_exc] = 2;
            e.p[P_st_strikeon] = 0; e.p[P_st_send] = 0; e.p[P_st_sustain] = 0.7f;
            e.p[P_st_bowforce] = 0.6f; e.p[P_st_exclvl] = ex; e.p[P_st_damp] = 0.3f;
            e.prepare (48000.0, 512); e.noteOn (45, 0.9f);
            render (e, 2.0); Take t = render (e, 2.0);
            float pk = 0; for (int i = 0; i < t.n(); ++i) pk = std::max (pk, std::abs (t.L[(size_t) i]));
            std::printf ("  EXCITER %.1f: bus peak %5.2f   raw body peak %5.2f   out %.2f\\n",
                         ex, e.stagePeak[Engine::ST_STRUCT], e.voices[0].st.rawPeak, pk);
        }

        /*  SIGNAL: two wavetables at full, which should not distort at all. */
        std::printf ("\\nSIGNAL: two tables at full level\\n");
        {
            Engine e; bareSine (e); e.p[P_m_on] = 0; e.p[P_s_on] = 1;
            e.p[P_s_wt1tab] = 0; e.p[P_s_wt1pos] = 0; e.p[P_s_wt1lvl] = 1.0f;
            e.p[P_s_wt2tab] = 0; e.p[P_s_wt2pos] = 0; e.p[P_s_wt2lvl] = 1.0f; e.p[P_s_wt2fine] = 0.5f; e.p[P_s_wt2oct] = 2;
            e.p[P_s_fmode] = 0; e.p[P_s_a_atk] = 0; e.p[P_s_send] = 0;
            e.prepare (48000.0, 512); e.noteOn (45, 1.0f); render (e, 1.0); Take t = render (e, 1.0);
            double fund = goertzel (t.L, 48000, 110.0, 0, t.n()), harm = 0;
            for (int k = 2; k <= 12; ++k) harm += goertzel (t.L, 48000, 110.0 * k, 0, t.n());
            std::printf ("  THD %6.1f dB   bus peak %.2f\\n", 10 * std::log10 ((harm + 1e-20) / (fund + 1e-20)), e.stagePeak[Engine::ST_SIGNAL]);
        }
    }
    return 0;
}`;
if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(p, s);

/* the raw body peak, before STRUCTURE's own output saturator */
const SP = "C:/Users/peter/b/ThirtyThousandYears/Source/Strata.h";
let st = fs.readFileSync(SP, "utf8");
const sa = `    int fractures = 0;           // counted, so a fracture shorter than a tick is still seen`;
const sb = `    int fractures = 0;           // counted, so a fracture shorter than a tick is still seen
    float rawPeak = 0.0f;        // the body BEFORE the output saturator, so its work can be measured`;
if (st.split(sa).length !== 2) { console.log("STRATA ANCHOR MISS"); process.exit(1); }
st = st.replace(sa, sb);
const sc = `            out[i] = ftanh (y * 1.2f) * aenv.tick() * aeGain * c.wmTrem;`;
const sd = `            rawPeak = std::max (std::abs (y), rawPeak * 0.999999f);
            out[i] = ftanh (y * 1.2f) * aenv.tick() * aeGain * c.wmTrem;`;
if (st.split(sc).length !== 2) { console.log("STRATA ANCHOR MISS 2"); process.exit(1); }
st = st.replace(sc, sd);
fs.writeFileSync(SP, st);
console.log("thd mode added");
