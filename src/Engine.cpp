#include "Engine.h"
#include "Viewport.h"
#include "Render.h"

#include <gsl/gsl>

#include "imgui.h"

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

void CEngine::OnRenderGui() const
{
	const ImVec2 size(400, 140);

	ImGui::SetNextWindowSize(size);
	
	ImGui::Begin("VkLearn", nullptr, ImGuiWindowFlags_NoResize);

	ImGui::Text("Welcome to VkLearn!\nGPU: %s", GetRender()->GetGpuName());

	ImGui::LabelText("FPS", "%.0f", m_FPS);

	ImGui::DragFloat("Rotation speed", &GetRender()->m_RotationSpeed, 1, 0, 100000, "%.2f deg/s");

	ImGui::End();
}

EEngineStatus CEngine::Initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = nullptr;

	ImGui::StyleColorsClassic();

	{
		// Source: https://github.com/ocornut/imgui/issues/707#issuecomment-252413954
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 5.3f;
		style.FrameRounding = 2.3f;
		style.ScrollbarRounding = 0;

		style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	}

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

	m_LastTime = std::chrono::high_resolution_clock::now();

	return EEngineStatus::Ok;
}

EEngineStatus CEngine::Update()
{
	using namespace  std::chrono;

	const high_resolution_clock::time_point now = high_resolution_clock::now();
	const duration<float> deltaTime = duration_cast<duration<float>>(now - m_LastTime);
	m_LastTime = now;
	const float fps = 1.f / deltaTime.count();
	if (m_FPSCount >= kFPSSampleCount)
	{
		m_FPS = m_FPSSum / static_cast<float>(m_FPSCount);
		m_FPSCount = m_FPSSum = 0;
	}
	else
	{
		m_FPSCount++;
		m_FPSSum += fps;
	}

	EEngineStatus status = m_Subsystems->Viewport.Update();

	if (status != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	if (!m_ShouldUpdate)return EEngineStatus::Ok;

	status = m_Subsystems->Render.Update(deltaTime.count());

	if (status != EEngineStatus::Ok)
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

	ImGui::DestroyContext();

	return EEngineStatus::Ok;
}
