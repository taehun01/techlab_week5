#pragma once

class FTimeManager final
{
public:

	static FTimeManager& Get()
	{
		static FTimeManager Instance;
		return Instance;
	}

	void Resume() { bIsRunning = true; }
	void Pause() { bIsRunning = false; }
	[[nodiscard]] float GetDeltaTime() const { return DeltaTime; }
	[[nodiscard]] LARGE_INTEGER GetFrequency() { return Frequency; }
	void Update();
	void SetTargetFPS(float InTargetFPS) { TargetFPS = InTargetFPS; }
	float GetTargetFPS() { return TargetFPS; }

	FTimeManager(const FTimeManager&) = delete;
	FTimeManager& operator=(const FTimeManager&) = delete;

	FTimeManager(FTimeManager&&) = delete;
	FTimeManager&& operator=(FTimeManager&&) = delete;

private:
	FTimeManager();
	~FTimeManager() = default;

	LARGE_INTEGER PrevTime;
	LARGE_INTEGER Frequency;
	
	// 프레임 제한에는 쓰지 않는다. 통계 그래프(FStatHistory)의 샘플 창 크기 기준값.
	float TargetFPS = 60.0f;
	float DeltaTime = 0.0f;

	bool bIsRunning = false;
};
