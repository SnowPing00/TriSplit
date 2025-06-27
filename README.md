EN
✨ The Core Principle of TriSplit
"Divide, Transform, and Conquer"
Instead of compressing a file all at once, TriSplit sees data as tiny 2-bit pieces (🧩 symbols).
It then intelligently sorts these pieces into three specialized containers (📂 streams) based on their characteristics:

value_bitmap (Pure Information): Collects only pure values, like from 01 or 10.

reconstructed_stream (Structural Information): Contains the overall skeleton and positional data.

auxiliary_mask (Exceptional Information): Records only the locations of special cases, like the rarer symbols 00 or 11.

Each of these separated streams becomes very simple and uniform, making them perfectly optimized for compression.
By compressing these individual streams, the engine achieves maximum overall efficiency.

KR
✨ TriSplit의 핵심 원리
"분리하고, 변환하고, 정복하라"
TriSplit은 파일을 통째로 압축하는 대신, 데이터를 2비트 단위의 작은 조각(🧩 심볼)으로 바라봅니다.
그리고 이 조각들을 성격에 따라 아래의 세 가지 특수 저장소(📂 스트림)로 재분배합니다.

value_bitmap (값 정보): 01 또는 10 처럼 순수한 값을 가진 정보만 모아둡니다.

reconstructed_stream (구조 정보): 데이터의 전체적인 뼈대와 위치 정보를 담습니다.

auxiliary_mask (예외 정보): 00이나 11 같이 가끔 등장하는 특별한 정보의 위치만 기록합니다.

이렇게 분리된 저장소들은 각각의 내용이 매우 단순해져, 압축하기 아주 좋은 상태가 됩니다.
이 최적화된 스트림들을 개별적으로 압축하여 최고의 효율을 만들어냅니다.
