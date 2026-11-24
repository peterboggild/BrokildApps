#!/usr/bin/env bash
# Merge a Journey to the Moon pull request and wait until it is live.
#   bash tools/merge.sh <pr number> "<squash commit title>" "<squash commit message>" [path to check under vr-apps/moon/]
# Waits for the checks on the PR's head commit (does not merge if one fails), squash-merges through the GitHub API
# with the token git already holds, then waits for the Pages deployment of the merge commit.
# The owner allowed merging this app's pull requests without asking once their tests pass (24 November 2026).
set -u
PR="$1"; TITLE="$2"; MSG="$3"; CHECK="${4:-launch.html}"
API=https://api.github.com/repos/peterboggild/BrokildApps
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill 2>/dev/null | sed -n 's/^password=//p')
H=(-H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json")
SHA=$(curl -s "${H[@]}" "$API/pulls/$PR" | python -c "import json,sys;print(json.load(sys.stdin)['head']['sha'])")
st=""
for i in $(seq 1 60); do
  st=$(curl -s "${H[@]}" "$API/commits/$SHA/check-runs" | python -c "import json,sys;r=json.load(sys.stdin)['check_runs'];print(' '.join(c['status']+':'+str(c['conclusion']) for c in r))")
  case "$st" in *in_progress*|*queued*|"") sleep 6;; *) break;; esac
done
echo "checks: $st"
case "$st" in *failure*|*cancelled*|*timed_out*|"") echo "NOT MERGED: checks did not pass"; exit 1;; esac
TITLE="$TITLE" MSG="$MSG" python -c "
import json,os
json.dump({'merge_method':'squash','commit_title':os.environ['TITLE'],'commit_message':os.environ['MSG']}, open(os.path.join(os.environ['TEMP'],'moon-merge.json'),'w'))"
M=$(curl -s -X PUT "${H[@]}" "$API/pulls/$PR/merge" --data-binary @"$TEMP/moon-merge.json" | python -c "import json,sys;d=json.load(sys.stdin);print(d.get('merged'),d.get('sha'),d.get('message'))")
echo "merge: $M"
case "$M" in True*) ;; *) exit 1;; esac
S7=$(echo "$M" | awk '{print substr($2,1,7)}')
for i in $(seq 1 90); do
  L=$(curl -s "$API/actions/runs?per_page=8" | S7="$S7" python -c "
import json,sys,os
for r in json.load(sys.stdin)['workflow_runs']:
  if r['head_sha'].startswith(os.environ['S7']) and r['name']=='pages build and deployment': print(r['status'],r['conclusion'])" 2>/dev/null)
  case "$L" in *completed*) echo "pages: $L"; break;; esac
  sleep 10
done
curl -s -o /dev/null -w "live check %{http_code} $CHECK\n" "https://peterboggild.github.io/BrokildApps/vr-apps/moon/$CHECK"
