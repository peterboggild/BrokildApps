# BrokildApps

Many small projects in one repo, served by GitHub Pages at `https://peterboggild.github.io/BrokildApps/`.
A push to `main` deploys the site (a Pages workflow, about two minutes).

## Working on the VR apps (`vr-apps/`)

**Planet Studio** (a new VR/PC app, not started): the specification and handover is `PLANET-STUDIO-DESIGN.md`.

Read `BROKILD-VR-HANDOVER.md` (the full handover: state, history, controls, open questions) and
`vr-apps/CLAUDE.md` before touching anything there. Together they hold the owner's working rules, how to test
without a headset, how the apps are built, and the mistakes already made once.

The short version of the rules, which the owner set by how he works:

- Reply in Danish, in plain language. He is not a programmer by trade here; explain what a thing does, not how.
- Work on a branch, open a pull request, and **merge only when he says so**. After a merge, check that the Pages
  deploy went through and tell him it is live.
- Tell him when a piece of work is finished (he asked for a notification, not silence).
- Always say what was tested only on a flat screen and what could not be tested in the headset. Nothing here can
  be tested in a headset from a cloud session; he tests on a Quest 3 and reports back.
