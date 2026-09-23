#include <windows.h>
#include "FTimeManager.h"

FTimeManager::FTimeManager()
{
    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&PrevTime);
    TargetFrameTime = 1.0f / TargetFPS;
}


void FTimeManager::Update()
{
    LARGE_INTEGER CurrentTime;
    QueryPerformanceCounter(&CurrentTime);

    float ActualDeltaTime =
        static_cast<float>(CurrentTime.QuadPart - PrevTime.QuadPart) /
        static_cast<float>(Frequency.QuadPart);

    while (ActualDeltaTime < TargetFrameTime)
    {
        QueryPerformanceCounter(&CurrentTime);

        ActualDeltaTime =
            static_cast<float>(CurrentTime.QuadPart - PrevTime.QuadPart) /
            static_cast<float>(Frequency.QuadPart);

        _mm_pause();
    }

    DeltaTime = bIsRunning ? ActualDeltaTime : 0.0f;
    PrevTime = CurrentTime;
}