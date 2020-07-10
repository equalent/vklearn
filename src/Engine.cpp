#include "Engine.h"
#include "Viewport.h"
#include "Render.h"

#include <gsl/gsl>

CEngine* gEngine = nullptr;

struct SEngineSubsystems
{
	CViewport Viewport;
	CRender Render;
};

int CEngine::Run(int argc, char** argv)
{
	static CEngine engine;
	gEngine = &engine;
	EEngineStatus status = engine.Initialize();

	if (status != EEngineStatus::Ok)
	{
		return 1;
	}

	while (engine.m_ShouldUpdate)
	{
		status = engine.Update();

		if (status != EEngineStatus::Ok)
		{
			engine.Shutdown();
			return 2;
		}
	}

	status = engine.Shutdown();
	if (status != EEngineStatus::Ok)
	{
		return 3;
	}

	return 0;
}

CViewport* CEngine::GetViewport() const
{
	return &m_Subsystems->Viewport;
}

CRender* CEngine::GetRender() const
{
	return &m_Subsystems->Render;
}

void CEngine::Quit()
{
	m_ShouldUpdate = false;
}

EEngineStatus CEngine::Initialize()
{
	m_Subsystems = new SEngineSubsystems();
	EEngineStatus status = m_Subsystems->Viewport.Initialize();

	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	status = m_Subsystems->Render.Initialize();
	
	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	return EEngineStatus::Ok;
}

EEngineStatus CEngine::Update()
{
	EEngineStatus status = m_Subsystems->Viewport.Update();

	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	if(!m_ShouldUpdate)return EEngineStatus::Ok;

	status = m_Subsystems->Render.Update();

	if(status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	return EEngineStatus::Ok;
}

EEngineStatus CEngine::Shutdown()
{
	auto _ = gsl::finally([&]
	{
		delete m_Subsystems;
		m_Subsystems = nullptr;
	});
	
	EEngineStatus status = m_Subsystems->Render.Shutdown();
	
	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	status = m_Subsystems->Viewport.Shutdown();

	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	return EEngineStatus::Ok;
}
