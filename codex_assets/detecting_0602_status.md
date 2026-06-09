# Detecting 0602 Status

Updated: Tue Jun  2 23:13:50 CST 2026

## Constraints
- Authoritative repo: af309:/astrum/home/hpchzy/code/TacVar
- Branch: dev/0526-manual
- af309 generates walking lists, edits, downloads, and plots only.
- Build/run only on c920bn3, cgnr6760pn2, camd9554n2.
- NUM_WALK=20, NTESTS=100, NTILES=100, NP_LIST=64 by default.
- Heartbeat: detecting-eval-full-0602, hourly, active.

## Walking Lists
-rw-rw-r-- 1 hpchzy hpchzy  92 Jun  2 22:15 scripts/walklists/detecting_expr1_fsize_Normal_n20_tbase1000.csv
-rw-rw-r-- 1 hpchzy hpchzy 112 Jun  2 23:08 scripts/walklists/detecting_expr1_timer_Normal_n20_tbase10000.csv
-rw-rw-r-- 1 hpchzy hpchzy 112 Jun  2 23:08 scripts/walklists/detecting_expr2_fsize_Normal_n20_tbase10000.csv
-rw-rw-r-- 1 hpchzy hpchzy  92 Jun  2 23:08 scripts/walklists/detecting_expr3_interval_Normal_n20_tbase1000.csv
-rw-rw-r-- 1 hpchzy hpchzy 112 Jun  2 23:08 scripts/walklists/detecting_expr3_interval_Normal_n20_tbase10000.csv
-rw-rw-r-- 1 hpchzy hpchzy 132 Jun  2 23:08 scripts/walklists/detecting_expr3_interval_Normal_n20_tbase100000.csv
-rw-rw-r-- 1 hpchzy hpchzy 112 Jun  2 23:08 scripts/walklists/detecting_expr4_frkern_Normal_n20_tbase10000.csv

## Launched Full Runs
### c920bn3
- Remote repo: c920bn3:~/code/TacVar
- Master log: c920bn3:~/code/data/20260602/c920bn3/outputDetecting/detecting_full_0602_master.log
- Output stamp: 20260602-full0602
  57967       01:10  0.0 run_detecting_f /bin/bash /tmp/run_detecting_full_0602.sh
  78402       00:08  0.1 run_entry_detec /bin/bash scripts/run_entry_detecting.expr2Fsize.0602.sh detect
  80224       00:00  0.0 run_entry_detec /bin/bash scripts/run_entry_detecting.expr2Fsize.0602.sh detect
### cgnr6760pn2
- Remote repo: cgnr6760pn2:~/code/TacVar
- Master log: cgnr6760pn2:~/code/data/20260602/cgnr6760pn2/outputDetecting/detecting_full_0602_master.log
- Output stamp: 20260602-full0602
1547374       01:10  0.0 run_detecting_f /bin/bash /tmp/run_detecting_full_0602.sh
1547384       01:10  0.0 run_entry_detec /bin/bash scripts/run_entry_detecting.expr1Timer.0602.sh detect
1548569       00:09  0.0 run_entry_detec /bin/bash scripts/run_entry_detecting.expr1Timer.0602.sh detect
### camd9554n2
- Remote repo: camd9554n2:~/code/TacVar
- Master log: camd9554n2:~/code/data/20260602/camd9554n2/outputDetecting/detecting_full_0602_master.log
- Output stamp: 20260602-full0602
 660509       01:10  0.0 run_detecting_f /bin/bash /tmp/run_detecting_full_0602.sh
 660519       01:10  0.2 run_entry_detec /bin/bash scripts/run_entry_detecting.expr1Timer.0602.sh detect
 680126 441077234-00:18:40 0.0 run_entry_detec /bin/bash scripts/run_entry_detecting.expr1Timer.0602.sh detect

## Heartbeat Note 2026-06-02 23:55 CST

- `c920bn3` and `camd9554n2` completed `expr1timer`, `expr2fsize`, and `expr3interval`; both were still progressing through `expr4frkern`.
- `cgnr6760pn2` stalled in `expr1timer/tsc_asym/w0015_ta10007`: `run.log` stopped after `Gauge info: gpns=0.453000`, with no CDF output after about 20 minutes.
- Minimal action taken: killed only the stuck `prterun` parent for that `tsc_asym/w0015` run. The outer script continued to `tsc_asym/w0016`; mark `cgnr6760pn2 expr1timer tsc_asym w0015` as incomplete/invalid unless rerun manually.

## Next Resume Commands
```bash
source ~/proxy.bash && ssh af309
source ~/proxy.bash
cd /astrum/home/hpchzy/code/TacVar
for h in c920bn3 cgnr6760pn2 camd9554n2; do
  ssh "$h" 'host=$(hostname -s); tail -n 40 ~/code/data/20260602/${host}/outputDetecting/detecting_full_0602_master.log'
done
# After completion, rsync each host outputDetecting back to af309 and manually fill DATASETS in scripts/plot_entry_detecting_full.0602.ipynb.
```

## Heartbeat Note 2026-06-03 00:22 CST

- Branch confirmed: dev/0526-manual.
- c920bn3: expr1timer=140, expr2fsize=1400, expr3interval=420 complete; expr4frkern=4109/5040 and running.
- camd9554n2: expr1timer=140, expr2fsize=1400, expr3interval=420 complete; expr4frkern=3282/5040 and running.
- cgnr6760pn2: recovered after previous tsc_asym w0015 kill; expr1timer=120/140 and running at papix6; expr2-4 not started yet.
- No data sync/plotting yet; wait for node completion, then rsync outputDetecting and fill notebook DATASETS manually.

## Heartbeat Note 2026-06-03 00:52 Sync Follow-up

- c920bn3 and camd9554n2 remote runs are complete for expr1-4.
- Began syncing c920bn3 outputDetecting to af309; partial copy reached about 3.4G and 3748 run.log files, then was intentionally stopped to avoid keeping this heartbeat turn open too long. Resume with rsync; existing files can be reused.
- camd9554n2 sync not started in this heartbeat because c920 sync was still running.
- cgnr6760pn2 still running expr2fsize; continue monitoring.
- Resume sync command: source ~/proxy.bash; for h in c920bn3 camd9554n2; do mkdir -p /astrum/home/hpchzy/code/data/20260602/$h; rsync -a $h:/home/hpchzy/code/data/20260602/$h/outputDetecting /astrum/home/hpchzy/code/data/20260602/$h/; done

## Heartbeat Note 2026-06-03 01:22 CST

- c920bn3/camd9554n2 remain complete remotely for expr1-4.
- cgnr6760pn2 stalled in expr2fsize clock_gettime/copy/fsize1024/w0002_ta9984 for over 30 min; run directory had no useful run.log/CDF. Minimal action: killed only prterun parent 1650878; outer script continued to w0003. Mark this single cgnr expr2fsize point incomplete/invalid unless rerun manually.
- Deferred full data sync this wake because previous c920 rsync was large; resume rsync after more compute progress or in a dedicated sync wake.

## Heartbeat Note 2026-06-03 01:52 CST

- c920bn3/camd9554n2 remote runs remain complete.
- cgnr6760pn2 is running expr2fsize and progressed to clock_gettime/copy/fsize8192/walk1; expr2fsize count=181/1400.
- Started/resumed af309 background sync for completed hosts with /tmp/sync_detecting_0602_completed.sh; log: codex_assets/detecting_0602_sync.log.

## Heartbeat Note 2026-06-03 02:22 CST

- af309 sync complete for finished hosts: c920bn3 and camd9554n2 local counts now match remote counts for expr1-4.
- cgnr6760pn2 progressed to expr2fsize mpi_wtime/copy/fsize16 but stalled at w0012_ta10012 with empty/no useful run.log. Minimal action: killed only prterun parent 1714935; mark this single point incomplete/invalid unless rerun manually.

## Heartbeat Note 2026-06-03 02:52 CST

- c920bn3/camd9554n2 local sync verified complete: expr1timer=140, expr2fsize=1400, expr3interval=420, expr4frkern=5040 for both hosts.
- cgnr6760pn2 continues normally after previous intervention: expr1timer=140 complete; expr2fsize=309/1400, currently mpi_wtime/copy/fsize512. No intervention this wake.

## Heartbeat Note 2026-06-03 03:22 CST

- cgnr6760pn2 continues normally: expr1timer=140 complete; expr2fsize=377/1400, currently mpi_wtime/copy/fsize4096. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 03:52 CST

- cgnr6760pn2 continues normally: expr1timer=140 complete; expr2fsize=452/1400, currently tsc/copy/fsize64. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 04:22 CST

- cgnr6760pn2 continues normally: expr1timer=140 complete; expr2fsize=525/1400, currently tsc/copy/fsize1024. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 04:52 CST

- cgnr6760pn2 continues normally: expr1timer=140 complete; expr2fsize=598/1400, currently tsc/copy/fsize8192. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 05:22 CST

- cgnr6760pn2 stalled in expr2fsize tsc_asym/copy/fsize16 around walk19 for about 27 min; no useful current walk directory/log output was present. Minimal action: killed only prterun parent 1891105; mark this single point incomplete/invalid unless rerun manually.

## Heartbeat Note 2026-06-03 05:52 CST

- cgnr6760pn2 continues after previous tsc_asym intervention: expr1timer=140 complete; expr2fsize=703/1400, currently tsc_asym/copy/fsize512. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 06:22 CST

- cgnr6760pn2 continues normally: expr1timer=140 complete; expr2fsize=939/1400, currently papi/copy/fsize1024. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 06:52 CST

- cgnr6760pn2 continues normally and is near the end of expr2fsize: expr1timer=140 complete; expr2fsize=1320/1400, currently likwid/copy/fsize512. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.

## Heartbeat Note 2026-06-03 08:00 CST

- cgnr6760pn2 completed expr2fsize and expr3interval: expr1timer=140, expr2fsize=1400, expr3interval=420.
- cgnr6760pn2 is running expr4frkern: 561/5040, currently clock_gettime/pow->pow after progressing through earlier frkern pairs. No intervention this wake.
- c920bn3/camd9554n2 remain complete and synced locally on af309.
- Next: continue monitoring cgnr expr4frkern; when complete, rsync cgnr outputDetecting to af309, fill notebook DATASETS paths, and run scripts/plot_entry_detecting_full.0602.ipynb.

## Manual Continue Note 2026-06-03

- cgnr6760pn2 expr4frkern is still running normally: 632/5040, currently clock_gettime/dgemm->add.
- expr1timer/expr2fsize/expr3interval are complete on cgnr; c920bn3/camd9554n2 are complete and synced locally.
- No sync/plot yet because cgnr expr4frkern is still incomplete.

## Heartbeat Note 2026-06-03 08:30 CST

- cgnr6760pn2 expr4frkern still running normally: 719/5040, currently clock_gettime/dgemm->dgemm. No intervention this wake.
- cgnr expr1timer/expr2fsize/expr3interval are complete; c920bn3/camd9554n2 are complete and synced locally.

## Stop Note 2026-06-03

- User requested stop. Deleted heartbeat automation detecting-eval-full-0602.
- Stopped current cgnr6760pn2 detecting run before expr4frkern completion. Existing partial data remains on cgnr and previously synced c920bn3/camd9554n2 data remains on af309.

## Stop Follow-up 2026-06-03

- Initial stop left cgnr expr4 child script running under init. Issued SIGKILL to /tmp/run_detecting_full_0602.sh, run_entry_detecting.expr*.0602.sh, TacVar/src/partes/partes-mpi.x, and matching prterun processes on cgnr6760pn2.

- Verification after force stop: see latest terminal check; no further heartbeat automation remains.
