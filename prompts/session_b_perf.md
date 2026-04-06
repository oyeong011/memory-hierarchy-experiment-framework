너는 Linux 성능 측정 자동화 엔지니어다.

다음 benchmark 프로젝트를 위한 perf 자동화 스크립트를 작성하라.

대상 프로젝트:
- `pointer_chase`
- `stream_lite`
- `stride_access`
- `row_col_traversal`

요구사항:
1. benchmark 실행 파일들을 순회하며 `perf stat`을 자동 수행
2. 다음 counter를 우선 사용
   - `cycles`
   - `instructions`
   - `cache-references`
   - `cache-misses`
   - `branch-misses`
3. 세부 cache counter가 가능하면 추가 수집
   - `L1-dcache-loads`
   - `L1-dcache-load-misses`
   - `LLC-loads`
   - `LLC-load-misses`
   - `dTLB-load-misses`
4. benchmark별로 problem size sweep 가능해야 함
5. stride benchmark는 stride 값도 함께 sweep
6. 결과를 CSV 파일로 저장
7. 실패한 perf 이벤트는 감지하여 경고를 남기고 가능한 이벤트만 계속 수집
8. 반복 측정(`-r`) 포함
9. `taskset`을 사용해 core pinning 예시 포함

출력물:
- `scripts/collect_perf.sh`
- `scripts/parse_perf.py`
- 결과 CSV 예시 스키마
- 사용법 README

주의:
- 실험 재현성을 위해 frequency governor, turbo boost, warmup 관련 체크리스트도 같이 적어라
- root 권한이 없어도 가능한 범위 우선
- `prompts/shared_conventions.md`의 CSV 스키마와 benchmark 이름을 그대로 사용하라
