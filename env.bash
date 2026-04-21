# Toolchain environment (best-effort, auto-detect).
# Rules:
# - Do NOT blindly append include/lib flags to CFLAGS/LDFLAGS if deps are missing.
# - If a dependency directory isn't present, export USE_* = 0 so build/scripts can skip it.
# - Users can override by exporting USE_* (0/1) before sourcing this file.

_pt_prepend_path() {
  local d="$1"
  [[ -d "${d}" ]] || return 0
  case ":${PATH}:" in
    *":${d}:"*) ;;
    *) export PATH="${d}:${PATH}" ;;
  esac
}

_pt_prepend_ldpath() {
  local d="$1"
  [[ -d "${d}" ]] || return 0
  case ":${LD_LIBRARY_PATH:-}:" in
    *":${d}:"*) ;;
    *) export LD_LIBRARY_PATH="${d}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" ;;
  esac
}

_pt_add_cflags_include() {
  local d="$1"
  [[ -d "${d}" ]] || return 0
  export CFLAGS="-I${d} ${CFLAGS:-}"
}

_pt_add_ldflags_lib() {
  local d="$1"
  shift
  [[ -d "${d}" ]] || return 0
  export LDFLAGS="-L${d} $* ${LDFLAGS:-}"
}

# Python (optional).
# IMPORTANT: do NOT auto-activate conda here. Conda often exports CC/CXX and may
# shadow system/cluster toolchains (mpicc, papi, likwid, gsl, ...).
# If you really want conda, do: `export USE_CONDA=1; source env.bash`.
if [[ "${USE_CONDA:-0}" == "1" && -f "${HOME}/miniconda3/bin/activate" ]]; then
  # shellcheck disable=SC1090
  source "${HOME}/miniconda3/bin/activate"
fi

# If conda was activated elsewhere, ensure it doesn't hijack the compiler toolchain.
unset CC CXX FC F77 F90 MPICC MPICXX OMPI_CC OMPI_CXX

# ===== MPI =====
: "${MPI_HOME:=/home/hpchzy/opt/openmpi-5.0.10}"
export MPI_HOME
if [[ -d "${MPI_HOME}/bin" ]]; then
  export USE_MPI="${USE_MPI:-1}"
  _pt_prepend_path "${MPI_HOME}/bin"
  _pt_prepend_ldpath "${MPI_HOME}/lib"
  # Prefer the MPI wrapper compiler from MPI_HOME.
  export CC="${MPI_HOME}/bin/mpicc"
else
  export USE_MPI="${USE_MPI:-0}"
fi

# ===== OpenBLAS =====
: "${OPENBLAS_HOME:=/home/hpchzy/opt/openblas-0.3.32}"
export OPENBLAS_HOME
if [[ -d "${OPENBLAS_HOME}/include" && -d "${OPENBLAS_HOME}/lib" ]]; then
  export USE_OPENBLAS="${USE_OPENBLAS:-1}"
  _pt_prepend_path "${OPENBLAS_HOME}/bin"
  _pt_add_cflags_include "${OPENBLAS_HOME}/include"
  _pt_add_ldflags_lib "${OPENBLAS_HOME}/lib" -lopenblas
  _pt_prepend_ldpath "${OPENBLAS_HOME}/lib"
else
  export USE_OPENBLAS="${USE_OPENBLAS:-0}"
fi

# ===== PAPI =====
: "${PAPI_HOME:=/home/hpchzy/opt/papi-7.2.0}"
export PAPI_HOME
if [[ -d "${PAPI_HOME}/include" && ( -d "${PAPI_HOME}/lib" || -d "${PAPI_HOME}/lib64" ) ]]; then
  export USE_PAPI="${USE_PAPI:-1}"
  _pt_prepend_path "${PAPI_HOME}/bin"
  _pt_add_cflags_include "${PAPI_HOME}/include"
  if [[ -d "${PAPI_HOME}/lib" ]]; then
    _pt_add_ldflags_lib "${PAPI_HOME}/lib" -lpapi
    _pt_prepend_ldpath "${PAPI_HOME}/lib"
  fi
  if [[ -d "${PAPI_HOME}/lib64" ]]; then
    _pt_add_ldflags_lib "${PAPI_HOME}/lib64" -lpapi
    _pt_prepend_ldpath "${PAPI_HOME}/lib64"
  fi
else
  export USE_PAPI="${USE_PAPI:-0}"
fi

# ===== LIKWID =====
: "${LIKWID_HOME:=/home/hpchzy/opt/likwid-5.5.1}"
export LIKWID_HOME
if [[ -d "${LIKWID_HOME}/include" && ( -d "${LIKWID_HOME}/lib" || -d "${LIKWID_HOME}/lib64" ) ]]; then
  export USE_LIKWID="${USE_LIKWID:-1}"
  _pt_prepend_path "${LIKWID_HOME}/bin"
  _pt_add_cflags_include "${LIKWID_HOME}/include"
  if [[ -d "${LIKWID_HOME}/lib" ]]; then
    _pt_add_ldflags_lib "${LIKWID_HOME}/lib" -llikwid
    _pt_prepend_ldpath "${LIKWID_HOME}/lib"
  fi
  if [[ -d "${LIKWID_HOME}/lib64" ]]; then
    _pt_add_ldflags_lib "${LIKWID_HOME}/lib64" -llikwid
    _pt_prepend_ldpath "${LIKWID_HOME}/lib64"
  fi
else
  export USE_LIKWID="${USE_LIKWID:-0}"
fi

# Optional: show detected tool versions if requested.
if [[ "${PT_SHOW_DEPS:-0}" == "1" ]]; then
  command -v mpicc >/dev/null 2>&1 && mpicc --version | head -n 1 || true
  command -v papi_version >/dev/null 2>&1 && papi_version || true
  command -v likwid-perfctr >/dev/null 2>&1 && likwid-perfctr -v || true
fi