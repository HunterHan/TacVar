# Surgical update for expr4 notebook cells

import json
import os

nb_path = os.path.expanduser('~/code/TacVar/scripts/plot_entry_detecting.expr4Frkern.0614.ipynb')
with open(nb_path, 'r') as f:
    nb = json.load(f)

# Cell 3: Data Loading update for expr4
nb['cells'][3]['source'] = [
    "# Data loading for expr4.frkern\n",
    "rows = []\n",
    "for root in RUN_ROOTS:\n",
    "    print(f\"Scanning {root}...\")\n",
    "    # for run_dir in root.glob(\"np*/timer*/interval*_fkern*_rkern*_fsize*_rsize*\"):\n",
    "    for run_dir in root.glob(\"**/w0001_*\"):\n",
    "        run_dir = run_dir.parent\n",
    "        meta = parse_meta(run_dir / \"meta.txt\")\n",
    "        host = meta.get(\"host\")\n",
    "        timer = meta.get(\"timer\")\n",
    "        interval = float(meta.get(\"interval_ns\", 0))\n",
    "        fsize = float(meta.get(\"fsize_kib\", 0))\n",
    "        fkern = meta.get(\"fkern\")\n",
    "        rkern = meta.get(\"rkern\")\n",
    "        \n",
    "        # Walk through walks\n",
    "        all_measured = []\n",
    "        all_theory = []\n",
    "        for walk_dir in run_dir.glob(\"w*\"):\n",
    "            w_meta = parse_meta(walk_dir / \"meta.txt\")\n",
    "            measured = read_values(walk_dir / \"ta_measured.csv\")\n",
    "            theory = [float(w_meta.get(\"ta\", 0))] * len(measured)\n",
    "            if measured:\n",
    "                all_measured.extend(measured)\n",
    "                all_theory.extend(theory)\n",
    "        \n",
    "        if all_measured:\n",
    "            rl, rh, ln, ld, hn, hd = rl_rh_detail(all_measured, all_theory)\n",
    "            rows.append({\n",
    "                \"host\": host, \"timer\": timer, \"interval_ns\": interval, \n",
    "                \"fsize_kib\": fsize, \"fkern\": fkern, \"rkern\": rkern,\n",
    "                \"rl\": rl, \"rh\": rh\n",
    "            })\n",
    "df = pd.DataFrame(rows)\n",
    "display(df.head())\n"]

# Cell 4: LaTeX Table Print
nb['cells'][4]['source'] = [
    "# Build Rl/Rh front-kernel x rear-kernel tables and print LaTeX.\n",
    "HOST_LABELS = {\n",
    "    'c920bn3': 'Kunpeng 920B',\n",
    "    'camd9554n1': 'AMD EPYC 9554',\n",
    "    'cgnr6760pn2': 'Intel Xeon 6760P',\n",
    "}\n",
    "KERNEL_TABLE_ORDER = ['copy', 'add', 'scale', 'triad', 'pow', 'dgemm']\n",
    "\n",
    "def latex_escape(s): return str(s).replace('_', r'\\_')\n",
    "def kernel_label(name): return latex_escape(str(name).upper())\n",
    "\n",
    "for (host, timer, interval_ns, fsize_kib), g in df.groupby(['host', 'timer', 'interval_ns', 'fsize_kib']):\n",
    "    print(f'\\n== {HOST_LABELS.get(host, host)} timer={timer} interval={int(interval_ns)}ns fsize={int(fsize_kib)}KiB ==')\n",
    "    rl_table = g.pivot_table(index='rkern', columns='fkern', values='rl', aggfunc='mean').reindex(index=KERNEL_TABLE_ORDER, columns=KERNEL_TABLE_ORDER)\n",
    "    rh_table = g.pivot_table(index='rkern', columns='fkern', values='rh', aggfunc='mean').reindex(index=KERNEL_TABLE_ORDER, columns=KERNEL_TABLE_ORDER)\n",
    "    \n",
    "    # Simple TeX print logic\n",
    "    print(r'\\begin{tabular}{l' + 'cc' * len(KERNEL_TABLE_ORDER) + '}')\n",
    "    header = ' & '.join([f'\\multicolumn{2}{c}{{{kernel_label(k)}}}' for k in KERNEL_TABLE_ORDER])\n",
    "    print(f' & {header} \\\\\\')\n",
    "    print(' & ' + ' & '.join(['$', '$'] * len(KERNEL_TABLE_ORDER)) + r' \\\\ \\midrule')\n",
    "    for rk in KERNEL_TABLE_ORDER:\n",
    "        row = [kernel_label(rk)]\n",
    "        for fk in KERNEL_TABLE_ORDER:\n",
    "            rl = rl_table.loc[rk, fk] if rk in rl_table.index and fk in rl_table.columns else float('nan')\n",
    "            rh = rh_table.loc[rk, fk] if rk in rh_table.index and fk in rh_table.columns else float('nan')\n",
    "            row.append('--' if pd.isna(rl) else f'{rl:.1f}')\n",
    "            row.append('--' if pd.isna(rh) else f'{rh:.1f}')\n",
    "        print(' & '.join(row) + r' \\\\')\n",
    "    print(r'\\end{tabular}')\n"]

with open(nb_path, 'w') as f:\n",
    "    json.dump(nb, f, indent=1)\n"
