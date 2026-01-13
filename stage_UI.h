#pragma once
class StageUI
{
	public:
	StageUI() = default;
	~StageUI() = default;
	void Initialize();
	void Update(double elapsed_time);
	void Draw();
	void StartTimer();
	void StopTimer();
	void ResetTimer();
	void StartCountdownTimer();
	void StopCountdownTimer();
	void ResetCountdownTimer();
	void UpdateAccuracy(int fireCount,int hitCount);
private:
	double m_Timer;
	double m_CountdownTimer;
	float m_Accuracy;
	int m_Points;
	int m_HitCounter;
	bool m_isTimerActive;
	int m_frameTexId;
};

