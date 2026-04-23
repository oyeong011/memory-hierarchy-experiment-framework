너는 컴퓨터시스템 분야 학부 연구 프로젝트 보고서를 작성하는 조교다.

다음 프로젝트의 기술보고서 초안을 작성하라.

프로젝트 제목:
“Linux perf 기반 메모리 접근 패턴 및 시스템 수준 메모리 병목 분석”

프로젝트 범위:
- pointer chasing latency
- STREAM-lite bandwidth
- stride access
- row-major vs column-major traversal
- optional: tiled matrix multiply

보고서 목적:
- 메모리 접근 패턴과 locality가 캐시/메모리 병목에 미치는 영향을 실증적으로 분석
- Linux perf 기반 counter 수집 및 해석
- 단순한 software-level optimization 가능성 논의

요구사항:
1. 보고서 목차 작성
2. 각 장별로 들어가야 할 핵심 내용 bullet 정리
3. 서론 초안 작성
4. background 장 초안 작성
5. methodology 장에서 실험 환경, benchmark, perf counter, 통제 조건을 어떻게 써야 하는지 초안 작성
6. results 장에 들어갈 그래프 캡션 템플릿 작성
7. discussion 장에 들어갈 논의 포인트 작성
8. limitation과 future work 초안 작성

스타일:
- 과장 금지
- 학부 연구 프로젝트 수준에서 현실적으로 작성
- “DRAM 내부 구조를 직접 측정했다” 같은 과장 표현 금지
- 가능하면 재현성, 통제조건, 해석의 한계를 강조
