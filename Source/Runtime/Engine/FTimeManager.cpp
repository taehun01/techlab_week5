#include <windows.h>
#include "FTimeManager.h"

FTimeManager::FTimeManager()
{
    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&PrevTime);
}


// 프레임 제한 없음: 이전 프레임과의 실제 경과 시간만 측정한다.
void FTimeManager::Update()
{
    LARGE_INTEGER CurrentTime;
    QueryPerformanceCounter(&CurrentTime);

    const float ActualDeltaTime =
        static_cast<float>(CurrentTime.QuadPart - PrevTime.QuadPart) /
        static_cast<float>(Frequency.QuadPart);

    DeltaTime = bIsRunning ? ActualDeltaTime : 0.0f;
    PrevTime = CurrentTime;
}