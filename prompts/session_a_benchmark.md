너는 시스템 프로그래밍 엔지니어다.

다음 프로젝트를 위한 C benchmark 코드를 작성하라.

프로젝트:
“Linux perf 기반 메모리 접근 패턴 및 시스템 수준 메모리 병목 분석”

이번 세션 목표:
아래 benchmark 4개의 최소 실행 가능한 버전을 각각 독립적인 C 파일로 작성

대상:
1. `src/pointer_chase.c`
2. `src/stream_lite.c`
3. `src/stride_access.c`
4. `src/row_col_traversal.c`

공통 요구사항:
- C11 기준
- gcc/clang에서 컴파일 가능
- `Makefile` 포함
- 각 프로그램은 CLI 인자로 problem size를 받도록 작성
- 실행시간 측정을 위한 기본 timer 포함
- 결과를 stdout에 CSV 한 줄 형식으로 출력
- 코드에 실험 목적과 주요 파라미터를 주석으로 설명
- 지나치게 복잡한 최적화는 넣지 말고, 실험용으로 명확하게 작성
- benchmark별로 어떤 locality 특성을 관찰하려는지 주석으로 명시

추가 요구:
- pointer chasing은 단순 for-loop 최적화에 죽지 않도록 `volatile` 또는 dependency를 적절히 사용
- stream-lite는 Copy / Add / Triad 중 최소 2개 포함
- stride benchmark는 stride를 인자로 받도록 구현
- row/column traversal은 2D 배열 순회 방식 차이를 비교할 수 있게 구현

출력:
1. 각 `.c` 파일 전체 코드
2. 공통 `Makefile`
3. 컴파일 명령 예시
4. 실행 예시
5. 예상되는 측정 포인트 요약

중요:
- `prompts/shared_conventions.md`의 파일명, 바이너리명, CSV 컬럼 순서를 따를 것
- 기존 디렉터리 구조와 충돌하지 않게 `src/`와 `bin/` 기준으로 작성할 것
