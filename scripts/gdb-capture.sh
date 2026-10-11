#!/usr/bin/env bash
# gdb-capture.sh — снять стек и карту памяти зависшего goatlyn

set -euo pipefail

PID="${1:-$(pgrep -x goatlyn | head -1)}"

if [[ -z "${PID}" ]]; then
    echo "goatlyn не запущен"
    exit 1
fi

if ! kill -0 "${PID}" 2>/dev/null; then
    echo "PID ${PID} не существует"
    exit 1
fi

OUTDIR="${HOME}/goatlyn-debug"
mkdir -p "${OUTDIR}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUTFILE="${OUTDIR}/capture-${PID}-${STAMP}.txt"

echo "Снимаю отладку с PID ${PID}"
echo "Результат: ${OUTFILE}"

sudo gdb -p "${PID}" -batch \
    -ex "set pagination off" \
    -ex "echo \n=== THREADS ===\n" \
    -ex "thread apply all bt" \
    -ex "echo \n=== MAPPINGS ===\n" \
    -ex "info proc mappings" \
    -ex "echo \n=== REGISTERS (main thread) ===\n" \
    -ex "info registers" \
    -ex "detach" \
    -ex "quit" \
    > "${OUTFILE}" 2>&1

echo "Готово. Проверь: ${OUTFILE}"
