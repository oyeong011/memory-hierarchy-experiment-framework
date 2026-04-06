너는 성능 데이터 분석가다.

다음 프로젝트의 perf/benchmark 결과 CSV를 받아 그래프를 그리는 Python 코드를 작성하라.

프로젝트:
“Linux perf 기반 메모리 접근 패턴 및 시스템 수준 메모리 병목 분석”

만들 그래프:
1. Memory Latency Staircase
2. Bandwidth vs Working Set Size
3. Cache Miss Rate vs Stride
4. Row-major vs Column-major 비교 그래프
5. optional: Tile Size Effect
6. optional: Roofline 초안

요구사항:
- `pandas` + `matplotlib` 사용
- 입력 CSV 스키마를 먼저 정의
- `benchmark`, `size_bytes`, `stride`, `tile_size`, `cycles`, `instructions`, `cache_misses` 등 컬럼을 처리
- miss rate, IPC 등 파생지표 계산 함수 포함
- 각 그래프를 개별 PNG로 저장
- 그래프 제목, 축 이름, 범례 명확히 표기
- 코드 구조를 함수형으로 나눌 것
- 결과가 없거나 일부 benchmark 데이터가 빠져도 에러 없이 동작하도록 작성

출력:
1. `scripts/plot_results.py` 전체 코드
2. 필요한 CSV 컬럼 설명
3. 그래프별 해석 포인트 한 줄씩

주의:
- `prompts/shared_conventions.md`의 perf CSV 컬럼을 기준으로 구현하라
