#pragma once

static const float kPi = 3.14159265359;
static const float kTwoPi = 2.0f * kPi;
static const float kHalfPi = 0.5f * kPi;
static const float kInvPi = 1.0f / kPi;

float3 EncodeXYZToRGB( float3 vec )
{
	return (vec + 1.0) * 0.5; 
}

float3 DecodeRGBToXYZ( float3 color )
{
	return (color * 2.0) - 1.0;
}

float3 GetCameraWorldPosition(float4x4 viewMatrix)
{
	float3x3 rotationMatrix = float3x3(
        viewMatrix[0].xyz,
        viewMatrix[1].xyz,
        viewMatrix[2].xyz 
    );
	
	float3 translation = viewMatrix._m03_m13_m23;
	
	float3 cameraPosition = mul(-translation, rotationMatrix);
	return cameraPosition;
}

float RangeMap( float inValue, float inStart, float inEnd, float outStart, float outEnd )
{
	float fraction = (inValue - inStart) / (inEnd - inStart);
	float outValue = outStart + fraction * (outEnd - outStart);
	return outValue;
}

float RangeMapClamped( float inValue, float inStart, float inEnd, float outStart, float outEnd )
{
	float fraction = saturate( (inValue - inStart) / (inEnd - inStart) );
	float outValue = outStart + fraction * (outEnd - outStart);
	return outValue;
}

float SmoothStep3(float t)
{
	return t * t * (3.0 - 2.0 * t);
}
