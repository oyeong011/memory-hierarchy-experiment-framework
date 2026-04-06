#!/usr/bin/env bash
set -euo pipefail

SESSION_NAME="${1:-memproj}"
CLIENT_TTY="${2:-}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

create_window() {
  local name="$1"
  tmux new-window -t "${SESSION_NAME}" -n "${name}" -c "${ROOT_DIR}"
}

if ! tmux has-session -t "${SESSION_NAME}" 2>/dev/null; then
  tmux new-session -d -s "${SESSION_NAME}" -n "00-hub" -c "${ROOT_DIR}"
  create_window "A-bench"
  create_window "B-perf"
  create_window "C-plot"
  create_window "D-report"
  create_window "E-wiki"
  create_window "F-repro"

  tmux send-keys -t "${SESSION_NAME}:00-hub" "cd '${ROOT_DIR}' && clear && printf 'Hub\\n\\nProject root: %s\\nPrompt index: prompts/README.md\\nShared conventions: prompts/shared_conventions.md\\nOps table: docs/ops/parallel_sessions.md\\n' '${ROOT_DIR}'" C-m
  tmux send-keys -t "${SESSION_NAME}:A-bench" "cd '${ROOT_DIR}' && clear && printf 'Session A\\nPrompt: prompts/session_a_benchmark.md\\nShared: prompts/shared_conventions.md\\n'" C-m
  tmux send-keys -t "${SESSION_NAME}:B-perf" "cd '${ROOT_DIR}' && clear && printf 'Session B\\nPrompt: prompts/session_b_perf.md\\nShared: prompts/shared_conventions.md\\n'" C-m
  tmux send-keys -t "${SESSION_NAME}:C-plot" "cd '${ROOT_DIR}' && clear && printf 'Session C\\nPrompt: prompts/session_c_plot.md\\nShared: prompts/shared_conventions.md\\n'" C-m
  tmux send-keys -t "${SESSION_NAME}:D-report" "cd '${ROOT_DIR}' && clear && printf 'Session D\\nPrompt: prompts/session_d_report.md\\n'" C-m
  tmux send-keys -t "${SESSION_NAME}:E-wiki" "cd '${ROOT_DIR}' && clear && printf 'Session E\\nPrompt: prompts/session_e_wiki.md\\n'" C-m
  tmux send-keys -t "${SESSION_NAME}:F-repro" "cd '${ROOT_DIR}' && clear && printf 'Session F\\nPrompt: prompts/session_f_scaled_repro.md\\n'" C-m
fi

if [[ -n "${CLIENT_TTY}" ]]; then
  tmux switch-client -c "${CLIENT_TTY}" -t "${SESSION_NAME}"
elif [[ -n "${TMUX:-}" ]]; then
  tmux switch-client -t "${SESSION_NAME}"
else
  printf 'Workspace ready. Attach with: tmux attach -t %s\n' "${SESSION_NAME}"
fi
