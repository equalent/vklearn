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

class CEngine
{
public:
	static int Run(int argc, char** argv);

	CViewport* GetViewport() const;
	CRender* GetRender() const;
	void Quit();
private:
	std::chrono::high_resolution_clock::time_point m_LastTime;
	bool m_ShouldUpdate = true;
	SEngineSubsystems* m_Subsystems = nullptr;
	
	EEngineStatus Initialize();
	EEngineStatus Update();
	EEngineStatus Shutdown();
};

extern CEngine* gEngine;