#!/usr/bin/env bash
set -euo pipefail

CLIENT_TTY="${1:-}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -z "${CLIENT_TTY}" ]]; then
  CLIENT_TTY="$(tmux list-clients -F '#{client_tty}' | head -n 1)"
fi

CURRENT_SESSION="$(tmux list-clients -F '#{client_tty} #{session_name}' | awk -v tty="${CLIENT_TTY}" '$1==tty {print $2; exit}')"
if [[ -z "${CURRENT_SESSION}" ]]; then
  printf 'Failed to resolve tmux session for client %s\n' "${CLIENT_TTY}" >&2
  exit 1
fi

kill_known_windows() {
  local name
  for name in mem-hub mem-p1 mem-p2 mem-grid; do
    tmux kill-window -t "${CURRENT_SESSION}:${name}" 2>/dev/null || true
  done
}

create_next_window() {
  local name="$1"
  local last_index next_index
  last_index="$(tmux list-windows -t "${CURRENT_SESSION}" -F '#{window_index}' | sort -n | tail -1)"
  next_index=$((last_index + 1))
  tmux new-window -d -t "${CURRENT_SESSION}:${next_index}" -n "${name}" -c "${ROOT_DIR}"
}

load_file_in_pane() {
  local pane_target="$1"
  local title="$2"
  local file_path="$3"
  tmux select-pane -t "${pane_target}" -T "${title}"
  tmux send-keys -t "${pane_target}" "cd '${ROOT_DIR}' && clear && printf '%s\nShared: prompts/shared_conventions.md\n\n' '${title}' && cat '${file_path}'" C-m
}

kill_known_windows
tmux kill-session -t memproj 2>/dev/null || true

create_next_window "mem-hub"
tmux send-keys -t "${CURRENT_SESSION}:mem-hub" "cd '${ROOT_DIR}' && clear && cat prompts/README.md && printf '\n\n' && cat prompts/shared_conventions.md && printf '\n\n' && cat docs/ops/parallel_sessions.md" C-m

create_next_window "mem-p1"
tmux split-window -h -t "${CURRENT_SESSION}:mem-p1" -c "${ROOT_DIR}"
tmux split-window -v -t "${CURRENT_SESSION}:mem-p1" -c "${ROOT_DIR}"
tmux split-window -v -t "${CURRENT_SESSION}:mem-p1" -c "${ROOT_DIR}"
tmux select-layout -t "${CURRENT_SESSION}:mem-p1" tiled
tmux set-window-option -t "${CURRENT_SESSION}:mem-p1" pane-border-status top >/dev/null

mapfile -t P1_PANES < <(tmux list-panes -t "${CURRENT_SESSION}:mem-p1" -F '#{pane_id}')
load_file_in_pane "${P1_PANES[0]}" "A-bench" "prompts/session_a_benchmark.md"
load_file_in_pane "${P1_PANES[1]}" "B-perf" "prompts/session_b_perf.md"
load_file_in_pane "${P1_PANES[2]}" "C-plot" "prompts/session_c_plot.md"
load_file_in_pane "${P1_PANES[3]}" "D-report" "prompts/session_d_report.md"

create_next_window "mem-p2"
tmux split-window -h -t "${CURRENT_SESSION}:mem-p2" -c "${ROOT_DIR}"
tmux select-layout -t "${CURRENT_SESSION}:mem-p2" even-horizontal
tmux set-window-option -t "${CURRENT_SESSION}:mem-p2" pane-border-status top >/dev/null

mapfile -t P2_PANES < <(tmux list-panes -t "${CURRENT_SESSION}:mem-p2" -F '#{pane_id}')
load_file_in_pane "${P2_PANES[0]}" "E-wiki" "prompts/session_e_wiki.md"
load_file_in_pane "${P2_PANES[1]}" "F-repro" "prompts/session_f_scaled_repro.md"

tmux switch-client -c "${CLIENT_TTY}" -t "${CURRENT_SESSION}:mem-p1"
tmux select-pane -t "${P1_PANES[0]}"
tmux display-message -c "${CLIENT_TTY}" "Memory project panes ready in session ${CURRENT_SESSION}"

tmux list-windows -t "${CURRENT_SESSION}"
