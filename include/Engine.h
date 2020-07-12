#pragma once
#include <cstdint>
#include <chrono>

enum class EEngineStatus : uint8_t
{
	Ok = 0,
	Failed = 1
};

class CViewport;
class CRender;
struct SEngineSubsystems;

const uint16_t kFPSSampleCount = 4096;

class CEngine
{
public:
	static int Run(int argc, char** argv);

	CViewport* GetViewport() const;
	CRender* GetRender() const;
	void Quit();
	void OnRenderGui() const;
private:
	std::chrono::high_resolution_clock::time_point m_LastTime;
	bool m_ShouldUpdate = true;
	SEngineSubsystems* m_Subsystems = nullptr;
	float m_FPSSum = 0.f;
	uint16_t m_FPSCount = 0;
	float m_FPS = 0.f;
	
	EEngineStatus Initialize();
	EEngineStatus Update();
	EEngineStatus Shutdown();
};

extern CEngine* gEngine;