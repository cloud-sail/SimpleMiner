#pragma once

// TODO later
// #pragma pack_matrix(row_major)

//------------------------------------------------------------------------------------------------
struct Light
{
	float4 	color;					// Alpha(w) is the light intensity in [0,1]
	float3 	worldPosition;			// 
	float  	padding;				//
	float3 	spotForward;			// Forward Normal for spotlights (zero for omnidirectional point-lights)
	float  	ambience;				// Portion of indirect light this source gives to objects in its affected volume
	float	innerRadius;
	float	outerRadius;
	float	innerDotThreshold;		// if dot with forward is greater than inner threshold, full strength; -1 for point lights
	float	outerDotThreshold;		// if dot with forward is less than outer threshold, full strength; -2 for point lights
};

#define MAX_LIGHTS 8
struct LightConstants
{
	float4 	sunColor; 				// Alpha (w) channel is intensity; parallel sunlight
	float3  sunNormal;				// Forward direction of parallel sunlight
	int		numLights;
	Light 	lightArray[MAX_LIGHTS];
};

//------------------------------------------------------------------------------------------------
struct EngineConstants
{
	int		debugInt;
	float	debugFloat;
	float2  padding;
};

//-----------------------------------------------------------------------------------------------
struct PerFrameConstants
{
	float3  resolution;
	float	time;

	float4	mouse;
};
// Shows how to use the mouse input (only left button supported):
//
//      mouse.xy  = mouse position during last button down
//  abs(mouse.zw) = mouse position during last button click
// sign(mouze.z)  = button is down
// sign(mouze.w)  = button is clicked

//------------------------------------------------------------------------------------------------
struct CameraConstants
{
	float4x4 	worldToCameraTransform;		// View transform
	float4x4 	cameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 	renderToClipTransform;		// Projection transform
	float3	 	cameraWorldPosition;       	// Camera World Position
	float		padding;
    float4x4	clipToWorldTransform;
};

//------------------------------------------------------------------------------------------------
struct ModelConstants
{
	float4x4 	modelToWorldTransform;		// Model transform
	float4 		modelColor;
};
