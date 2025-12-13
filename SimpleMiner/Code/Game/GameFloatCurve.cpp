#include "Game/GameFloatCurve.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/implot/implot.h"

#include <algorithm>
#include <string>

//-----------------------------------------------------------------------------------------------
// Color Palette for visual clarity
//-----------------------------------------------------------------------------------------------
static const ImVec4 COLOR_CURVE = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);        // Blue
static const ImVec4 COLOR_KEY_POINT = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);    // Orange
static const ImVec4 COLOR_ARRIVE_TANGENT = ImVec4(0.3f, 0.9f, 0.4f, 1.0f); // Green
static const ImVec4 COLOR_LEAVE_TANGENT = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
static const ImVec4 COLOR_TANGENT_LINE = ImVec4(0.7f, 0.7f, 0.7f, 0.6f);  // Gray

//-----------------------------------------------------------------------------------------------
// SplinePoint1DWithID Implementation
//-----------------------------------------------------------------------------------------------
SplinePoint1DWithID::SplinePoint1DWithID(int id, float inputKey, float outputValue, float arriveTangent /*= 0.f*/, float leaveTangent /*= 0.f*/, CurveMode mode /*= CurveMode::LINEAR*/)
	: id(id)
	, inputKey(inputKey)
	, outputValue(outputValue)
	, arriveTangent(arriveTangent)
	, leaveTangent(leaveTangent)
	, mode(mode)
{
}

SplinePoint1D SplinePoint1DWithID::ToSplinePoint1D() const
{
	return SplinePoint1D(inputKey, outputValue, arriveTangent, leaveTangent, mode);
}

SplinePoint1DWithID SplinePoint1DWithID::FromSplinePoint1D(int id, SplinePoint1D const& point)
{
	return SplinePoint1DWithID(id, point.m_inputKey, point.m_outputValue, point.m_arriveTangent, point.m_leaveTangent, point.m_mode);
}

//-----------------------------------------------------------------------------------------------
// Spline1DEditor Implementation
//-----------------------------------------------------------------------------------------------
Spline1DEditor::Spline1DEditor()
{
	// Initialize with a simple default curve
	m_points.push_back(SplinePoint1DWithID(m_nextPointID++, 0.f, 0.f, 0.f, 0.f, CurveMode::CURVE));
	m_points.push_back(SplinePoint1DWithID(m_nextPointID++, 1.f, 1.f, 0.f, 0.f, CurveMode::CURVE));
	m_needsSortUpdate = true;
	m_needsPlotUpdate = true;
}

Spline1DEditor::Spline1DEditor(Spline1D const& spline)
{
	SetSpline(spline);
}

//-----------------------------------------------------------------------------------------------
Spline1D Spline1DEditor::GetSpline() const
{
	// Convert internal points to Spline1D
	Spline1D result;

	// Create a sorted copy
	std::vector<SplinePoint1DWithID> sortedPoints = m_points;
	std::sort(sortedPoints.begin(), sortedPoints.end(),
		[](SplinePoint1DWithID const& a, SplinePoint1DWithID const& b) {
			return a.inputKey < b.inputKey;
		});

	for (SplinePoint1DWithID const& point : sortedPoints)
	{
		result.AddPoint(point.ToSplinePoint1D());
	}

	return result;
}

void Spline1DEditor::SetSpline(Spline1D const& spline)
{
	m_points.clear();
	m_nextPointID = 0;
	m_selectedPointID = -1;

	int numPoints = spline.GetNumberOfSplinePoints();
	m_points.reserve(numPoints);

	for (int i = 0; i < numPoints; ++i)
	{
		CurvePointFloat const& curvePoint = spline.m_value.m_points[i];
		SplinePoint1D splinePoint(curvePoint.m_inputKey, curvePoint.m_outputValue,
			curvePoint.m_arriveTangent, curvePoint.m_leaveTangent, curvePoint.m_mode);
		m_points.push_back(SplinePoint1DWithID::FromSplinePoint1D(m_nextPointID++, splinePoint));
	}

	m_needsSortUpdate = true;
	m_needsPlotUpdate = true;
}

void Spline1DEditor::SetInitialRangeLimits(Vec2 mins, Vec2 maxs)
{
	m_initialRangeMins = mins;
	m_initialRangeMaxs = maxs;
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderUI(const char* windowName)
{
	if (ImGui::Begin(windowName))
	{
		RenderToolbar();
		ImGui::Separator();
		RenderPointsList();
	}
	ImGui::End();
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderPlot(const char* plotName, Vec2 const& size)
{
	if (m_needsPlotUpdate)
	{
		UpdatePlotData();
		m_needsPlotUpdate = false;
	}

	if (ImPlot::BeginPlot(plotName, ImVec2(size.x, size.y)))
	{

		ImPlot::SetupAxes("Input Key (t)", "Output Value");
		ImPlot::SetupAxesLimits(m_initialRangeMins.x, m_initialRangeMaxs.x, m_initialRangeMins.y, m_initialRangeMaxs.y, ImGuiCond_Once);
		

		// Render in order: tangent handles -> curve -> key points (so points are on top)
		if (m_showTangentHandles)
		{
			RenderTangentHandles();
		}

		RenderPlotCurve();

		if (m_showKeyPoints)
		{
			RenderKeyPoints();
		}

		ImPlot::EndPlot();
	}
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderToolbar()
{
	ImGui::Text("Curve Editor Controls");

	if (ImGui::Button("Add Point"))
	{
		AddPoint();
	}

	ImGui::Separator();

	// New point settings
	ImGui::Text("New Point Settings:");
	ImGui::PushItemWidth(120);
	ImGui::DragFloat("##NewInputKey", &m_newPointInputKey, 0.01f);
	ImGui::SameLine();
	ImGui::Text("Input Key");

	ImGui::DragFloat("##NewValue", &m_newPointValue, 0.01f);
	ImGui::SameLine();
	ImGui::Text("Value");
	ImGui::PopItemWidth();

	ImGui::Separator();

	// Visualization options
	ImGui::Checkbox("Show Tangent Handles", &m_showTangentHandles);
	ImGui::SameLine();
	ImGui::Checkbox("Show Key Points", &m_showKeyPoints);

	ImGui::PushItemWidth(120);
	if (ImGui::SliderInt("Plot Subdivisions", &m_plotSubdivisions, 10, 500))
	{
		m_needsPlotUpdate = true;
	}

	if (ImGui::SliderFloat("Tangent Length", &m_tangentHandleLength, 0.05f, 1.0f))
	{
		m_needsPlotUpdate = true;
	}
	ImGui::PopItemWidth();

	ImGui::Text("Total Points: %d", (int)m_points.size());
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderPointsList()
{
	if (m_needsSortUpdate)
	{
		UpdateSortedIndices();
		m_needsSortUpdate = false;
	}

	ImGui::Text("Control Points (sorted by Input Key):");

	if (m_points.empty())
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No points in curve");
		return;
	}

	// Create a child window for scrollable list
	ImGui::BeginChild("PointsList", ImVec2(0, 300), true);

	// Track if we need to break out of loop due to structural changes
	bool structureChanged = false;

	// Iterate through sorted indices
	for (int displayIndex = 0; displayIndex < (int)m_sortedIndices.size() && !structureChanged; ++displayIndex)
	{
		int dataIndex = m_sortedIndices[displayIndex];

		// Safety check: ensure dataIndex is valid
		if (dataIndex < 0 || dataIndex >= (int)m_points.size())
			continue;

		SplinePoint1DWithID& point = m_points[dataIndex];

		// Use point ID for ImGui identity
		ImGui::PushID(point.id);

		bool isSelected = (point.id == m_selectedPointID);

		// Display with index and input key for clarity
		char label[64];
		snprintf(label, sizeof(label), "Point %d (t=%.3f)", displayIndex, point.inputKey);

		if (ImGui::Selectable(label, isSelected))
		{
			m_selectedPointID = point.id;
		}

		if (isSelected)
		{
			ImGui::Indent();

			// Check if structure will change
			int oldSize = (int)m_points.size();
			RenderPointEditor(point.id);
			int newSize = (int)m_points.size();

			if (oldSize != newSize)
			{
				// Structure changed (point added or removed)
				structureChanged = true;
			}

			ImGui::Unindent();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderPointEditor(int pointID)
{
	SplinePoint1DWithID* point = FindPointByID(pointID);
	if (!point)
		return;

	bool changed = false;

	ImGui::PushItemWidth(150);

	// Input Key - can be freely edited, will auto-sort in display
	if (ImGui::DragFloat("Input Key", &point->inputKey, 0.01f))
	{
		changed = true;

		m_needsSortUpdate = true; 
	}

	// Output Value
	if (ImGui::DragFloat("Value", &point->outputValue, 0.01f))
	{
		changed = true;
	}

	// Mode selector
	const char* modes[] = { "Linear", "Curve" };
	int currentMode = (int)point->mode;
	if (ImGui::Combo("Mode", &currentMode, modes, 2))
	{
		point->mode = (CurveMode)currentMode;
		changed = true;
	}

	// Tangents (only visible in CURVE mode)
	//if (point->mode == CurveMode::CURVE)
	//{
	if (ImGui::DragFloat("Arrive Tangent", &point->arriveTangent, 0.01f))
	{
		changed = true;
	}

	if (ImGui::DragFloat("Leave Tangent", &point->leaveTangent, 0.01f))
	{
		changed = true;
	}

	// Unified tangent option
	if (ImGui::Button("Make Continuous"))
	{
		float avgTangent = (point->arriveTangent + point->leaveTangent) * 0.5f;
		point->arriveTangent = avgTangent;
		point->leaveTangent = avgTangent;
		changed = true;
	}
	//}

	ImGui::PopItemWidth();

	// Action buttons
	ImGui::Spacing();
	if (ImGui::Button("Duplicate"))
	{
		DuplicatePoint(pointID);
		// Note: DuplicatePoint now updates indices immediately
		changed = true;
	}

	ImGui::SameLine();
	if (ImGui::Button("Remove") && m_points.size() > 1)
	{
		RemovePoint(pointID);
		// Note: RemovePoint now updates indices immediately
		changed = true;
	}

	// Debug info
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "ID: %d", pointID);

	if (changed)
	{
		m_needsPlotUpdate = true;
	}
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderPlotCurve()
{
	if (m_plotInputKeys.empty() || m_plotValues.empty())
		return;

	ImPlot::SetNextLineStyle(COLOR_CURVE, 2.0f);
	ImPlot::PlotLine("Curve", m_plotInputKeys.data(), m_plotValues.data(), (int)m_plotInputKeys.size());
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderKeyPoints()
{
	if (m_points.empty())
		return;

	std::vector<float> keyInputs;
	std::vector<float> keyValues;
	keyInputs.reserve(m_points.size());
	keyValues.reserve(m_points.size());

	for (SplinePoint1DWithID const& point : m_points)
	{
		keyInputs.push_back(point.inputKey);
		keyValues.push_back(point.outputValue);
	}

	ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 8, COLOR_KEY_POINT, 2.0f, COLOR_KEY_POINT);
	ImPlot::PlotScatter("Key Points", keyInputs.data(), keyValues.data(), (int)m_points.size());
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RenderTangentHandles()
{
	for (SplinePoint1DWithID const& point : m_points)
	{
		// Only draw tangent handles for CURVE mode
		//if (point.mode != CurveMode::CURVE)
		//	continue;

		float t = point.inputKey;
		float v = point.outputValue;

		// Calculate handle positions
		Vec2 arriveHandle = CalculateTangentHandlePosition(t, v, point.arriveTangent, false);
		Vec2 leaveHandle = CalculateTangentHandlePosition(t, v, point.leaveTangent, true);

		std::string idStr = std::to_string(point.id);

		// Draw arrive tangent (left side, green)
		{
			float arriveLineX[2] = { t, arriveHandle.x };
			float arriveLineY[2] = { v, arriveHandle.y };

			ImPlot::SetNextLineStyle(COLOR_TANGENT_LINE, 1.5f);
			ImPlot::PlotLine(("##ArriveLine" + idStr).c_str(), arriveLineX, arriveLineY, 2);

			ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5, COLOR_ARRIVE_TANGENT, 1.5f, COLOR_ARRIVE_TANGENT);
			ImPlot::PlotScatter(("##ArriveHandle" + idStr).c_str(), &arriveHandle.x, &arriveHandle.y, 1);
		}

		// Draw leave tangent (right side, red)
		{
			float leaveLineX[2] = { t, leaveHandle.x };
			float leaveLineY[2] = { v, leaveHandle.y };

			ImPlot::SetNextLineStyle(COLOR_TANGENT_LINE, 1.5f);
			ImPlot::PlotLine(("##LeaveLine" + idStr).c_str(), leaveLineX, leaveLineY, 2);

			ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5, COLOR_LEAVE_TANGENT, 1.5f, COLOR_LEAVE_TANGENT);
			ImPlot::PlotScatter(("##LeaveHandle" + idStr).c_str(), &leaveHandle.x, &leaveHandle.y, 1);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::AddPoint()
{
	SplinePoint1DWithID newPoint(
		m_nextPointID++,
		m_newPointInputKey,
		m_newPointValue,
		0.f,
		0.f,
		CurveMode::CURVE
	);

	m_points.push_back(newPoint);
	m_selectedPointID = newPoint.id;

	// Immediately update sort indices to keep consistency
	UpdateSortedIndices();
	m_needsSortUpdate = false;
	m_needsPlotUpdate = true;

	// Increment for next point
	m_newPointInputKey += 0.5f;
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::RemovePoint(int pointID)
{
	int index = FindPointIndexByID(pointID);
	if (index < 0)
		return;

	m_points.erase(m_points.begin() + index);

	if (m_selectedPointID == pointID)
	{
		m_selectedPointID = -1;
	}

	// Immediately update sort indices to keep consistency
	UpdateSortedIndices();
	m_needsSortUpdate = false;
	m_needsPlotUpdate = true;
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::DuplicatePoint(int pointID)
{
	SplinePoint1DWithID* original = FindPointByID(pointID);
	if (!original)
		return;

	// Create duplicate with slightly offset input key and new ID
	SplinePoint1DWithID duplicate(
		m_nextPointID++,
		original->inputKey + 0.1f,
		original->outputValue,
		original->arriveTangent,
		original->leaveTangent,
		original->mode
	);

	m_points.push_back(duplicate);
	m_selectedPointID = duplicate.id;

	// Immediately update sort indices to keep consistency
	UpdateSortedIndices();
	m_needsSortUpdate = false;
	m_needsPlotUpdate = true;
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::UpdateSortedIndices()
{
	m_sortedIndices.clear();
	m_sortedIndices.reserve(m_points.size());

	// Fill with indices
	for (int i = 0; i < (int)m_points.size(); ++i)
	{
		m_sortedIndices.push_back(i);
	}

	// Sort indices by inputKey
	std::sort(m_sortedIndices.begin(), m_sortedIndices.end(),
		[this](int a, int b) {
			return m_points[a].inputKey < m_points[b].inputKey;
		});
}

//-----------------------------------------------------------------------------------------------
SplinePoint1DWithID* Spline1DEditor::FindPointByID(int id)
{
	for (SplinePoint1DWithID& point : m_points)
	{
		if (point.id == id)
			return &point;
	}
	return nullptr;
}

//-----------------------------------------------------------------------------------------------
int Spline1DEditor::FindPointIndexByID(int id) const
{
	for (int i = 0; i < (int)m_points.size(); ++i)
	{
		if (m_points[i].id == id)
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------------------------
Vec2 Spline1DEditor::CalculateTangentHandlePosition(float inputKey, float outputValue, float tangent, bool isLeave) const
{
	// Tangent is the derivative dy/dt (slope)
	// Handle position: offset from key point along the tangent direction

	float direction = isLeave ? 1.0f : -1.0f;
	float deltaT = m_tangentHandleLength * direction;
	float deltaV = tangent * deltaT;

	return Vec2(inputKey + deltaT, outputValue + deltaV);
}

//-----------------------------------------------------------------------------------------------
void Spline1DEditor::UpdatePlotData()
{
	// Convert to Spline1D and evaluate
	Spline1D spline = GetSpline();

	int numPoints = spline.GetNumberOfSplinePoints();

	if (numPoints == 0)
	{
		m_plotInputKeys.clear();
		m_plotValues.clear();
		return;
	}

	if (numPoints == 1)
	{
		m_plotInputKeys.resize(1);
		m_plotValues.resize(1);
		m_plotInputKeys[0] = spline.m_value.m_points[0].m_inputKey;
		m_plotValues[0] = spline.m_value.m_points[0].m_outputValue;
		return;
	}

	int numSegments = spline.GetNumberOfSplineSegments();
	int totalSamples = numSegments * m_plotSubdivisions + 1;

	m_plotInputKeys.clear();
	m_plotValues.clear();
	m_plotInputKeys.reserve(totalSamples);
	m_plotValues.reserve(totalSamples);

	float startKey = spline.m_value.m_points[0].m_inputKey;
	float endKey = spline.m_value.m_points[numPoints - 1].m_inputKey;
	float range = endKey - startKey;

	if (range <= 0.f)
	{
		m_plotInputKeys.push_back(startKey);
		m_plotValues.push_back(spline.GetValueAtInputKey(startKey));
		return;
	}

	for (int i = 0; i <= numSegments * m_plotSubdivisions; ++i)
	{
		float t = (float)i / (float)(numSegments * m_plotSubdivisions);
		float inputKey = startKey + t * range;
		float value = spline.GetValueAtInputKey(inputKey);

		m_plotInputKeys.push_back(inputKey);
		m_plotValues.push_back(value);
	}
}