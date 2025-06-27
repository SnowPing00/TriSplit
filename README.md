EN
The core principle of TriSplit is "Divide, Transform, and Conquer."

Instead of compressing the original data directly, the engine treats it as a sequence of 2-bit symbols (00, 01, 10, 11). It then intelligently separates these symbols into three different sub-streams based on their statistical properties.

value_bitmap (Pure Information): A stream containing only the value information from 01 and 10 symbols.

reconstructed_stream (Structural Information): A stream that holds the data's overall structure and the positions of symbols.

auxiliary_mask (Exceptional Information): A stream that only records the locations of the rarer symbol between 00 and 11.

Each of these separated streams becomes very simple in its own way, making it ideal for compression. By individually compressing these optimized streams, the engine achieves high overall efficiency.

KR
TriSplit의 핵심 원리는 **"분리하고, 변환하고, 정복하라"**입니다.

이 압축기는 원본 데이터를 직접 압축하는 대신, 데이터를 2비트 단위의 작은 심볼(00, 01, 10, 11)로 간주합니다. 그 후, 이 심볼들을 통계적 특성에 따라 세 종류의 스트림으로 지능적으로 분리합니다.

Value Bitmap (순수 정보): 01, 10 심볼의 값 정보만 모아놓은 스트림.

Reconstructed Stream (구조 정보): 데이터의 전체적인 구조와 심볼의 위치 정보를 담은 스트림.

Auxiliary Mask (예외 정보): 00과 11 중 더 드물게 나타나는 심볼의 위치만 따로 기록한 스트림.

이렇게 분리된 스트림들은 각각의 특성이 매우 단순해져 압축에 아주 유리한 상태가 됩니다. 이 최적화된 스트림들을 개별적으로 압축하여 전체적으로 높은 효율을 달성하는 방식입니다.
