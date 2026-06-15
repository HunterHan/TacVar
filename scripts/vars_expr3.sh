EXPR_NAME="detecting.expr3.interval"
OUT_KIND="expr3interval"
TBASE_LIST="${TBASE_LIST:-1000 10000 100000 1000000 10000000}"
FSIZE_KIB="${FSIZE_KIB:-0}"
FKERN="${FKERN:-copy}"
RKERN="${RKERN:-none}"
RSIZE_KIB="${RSIZE_KIB:-0}"
SHUFFLE_COUNT="${SHUFFLE_COUNT:-3}"
SHUFFLE_SEED="${SHUFFLE_SEED:-0614}"

walk_for_tbase(){ echo "${WALK_ROOT}/detecting_expr3_interval_Normal_n${NUM_WALK}_tbase${1}.csv"; }
