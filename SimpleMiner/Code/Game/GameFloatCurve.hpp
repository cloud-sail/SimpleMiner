#pragma once

#include "Engine/Math/Spline.hpp"

// Known Issue: When Curve Mode is Linear, it need to affect current leave tangent and next arrive tangent.

#include <string>
#include <vector>

//-----------------------------------------------------------------------------------------------
// Internal structure: Spline point with unique ID
//-----------------------------------------------------------------------------------------------
struct SplinePoint1DWithID
{
	int id = -1;
	float inputKey = 0.f;
	float outputValue = 0.f;
	float arriveTangent = 0.f;
	float leaveTangent = 0.f;
	CurveMode mode = CurveMode::LINEAR;

	SplinePoint1DWithID() = default;
	SplinePoint1DWithID(int id, float inputKey, float outputValue, float arriveTangent = 0.f, float leaveTangent = 0.f, CurveMode mode = CurveMode::LINEAR);

	// Convert to/from SplinePoint1D
	SplinePoint1D ToSplinePoint1D() const;
	static SplinePoint1DWithID FromSplinePoint1D(int id, SplinePoint1D const& point);
};

//-----------------------------------------------------------------------------------------------
// Spline1D Editor with ImGui/ImPlot visualization
// - Edit control points via ImGui UI
// - Visualize curve, key points, and tangent handles via ImPlot
// - Points have unique IDs, display order is sorted by inputKey
//-----------------------------------------------------------------------------------------------
class Spline1DEditor
{
public:
	Spline1DEditor();
	explicit Spline1DEditor(Spline1D const& spline);

	// Main rendering functions
	void RenderUI(const char* windowName = "Spline1D Editor");
	void RenderPlot(const char* plotName = "Spline Curve", Vec2 const& size = Vec2(-1, 300));

	// Accessors - converts internal data to Spline1D
	Spline1D GetSpline() const;
	void SetSpline(Spline1D const& spline);

	// Configuration
	void SetPlotSubdivisions(int subdivisions) { m_plotSubdivisions = subdivisions; }
	void SetTangentHandleLength(float length) { m_tangentHandleLength = length; }
	void SetAutoFitView(bool autoFit) { m_autoFitView = autoFit; }

	void SetInitialRangeLimits(Vec2 mins, Vec2 maxs);

private:
	// UI Components
	void RenderToolbar();
	void RenderPointsList();
	void RenderPointEditor(int pointID);

	// Plot Components
	void RenderPlotCurve();
	void RenderKeyPoints();
	void RenderTangentHandles();

	// Utilities
	void AddPoint();
	void RemovePoint(int pointID);
	void DuplicatePoint(int pointID);
	void UpdateSortedIndices();

	SplinePoint1DWithID* FindPointByID(int id);
	int FindPointIndexByID(int id) const;

	Vec2 CalculateTangentHandlePosition(float inputKey, float outputValue, float tangent, bool isLeave) const;
	void UpdatePlotData();

private:
	// Data storage (unsorted, identified by unique IDs)
	std::vector<SplinePoint1DWithID> m_points;
	int m_nextPointID = 0; // Auto-increment ID generator

	// Display order (sorted by inputKey)
	std::vector<int> m_sortedIndices; // Indices into m_points array
	bool m_needsSortUpdate = true;

	// Selection state (tracked by ID)
	int m_selectedPointID = -1;

	// Plot settings
	int m_plotSubdivisions = 25;
	float m_tangentHandleLength = 0.3f;
	bool m_autoFitView = true;

	// Cached plot data
	std::vector<float> m_plotInputKeys;
	std::vector<float> m_plotValues;
	bool m_needsPlotUpdate = true;

	// UI state
	bool m_showTangentHandles = true;
	bool m_showKeyPoints = true;
	float m_newPointInputKey = 0.f;
	float m_newPointValue = 0.f;

	// Initial Axis X Y Range Limits
	Vec2 m_initialRangeMins = Vec2(-1.f, -1.f);
	Vec2 m_initialRangeMaxs = Vec2(1.f, 1.f);
};

/*
#include "Spline1DEditor.hpp"

class MyApp
{
private:
	Spline1DEditor m_curveEditor;
};

void MyApp::Startup()
{
	Spline1D initialCurve;
	initialCurve.SetFromCatmullRomAlgorithm({0.f, 0.5f, 1.0f, 0.8f, 0.2f});
	m_curveEditor.SetSpline(initialCurve);
}

void MyApp::RenderImGui()
{
	m_curveEditor.RenderUI("My Curve Editor");

	if (ImGui::Begin("Curve Visualization"))
	{
		m_curveEditor.RenderPlot("Animation Curve", Vec2(-1, 400));
	}
	ImGui::End();

	Spline1D const& curve = m_curveEditor.GetSpline();
	float value = curve.GetValueAtInputKey(currentTime);
}


*/