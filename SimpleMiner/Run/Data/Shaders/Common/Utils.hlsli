#pragma once

static const uint INVALID_INDEX = uint(-1); // uint32_t(-1) 4294967295

// Please ensure the engine will set default texture to this index
// if the index is invalid use the index below


static const uint DEFAULT_DIFFUSE_TEXTURE_INDEX = 0; 
static const uint DEFAULT_NORMAL_TEXTURE_INDEX = 1; 

uint GetSafeIndex(uint index, uint defaultIndex)
{
    return (index != INVALID_INDEX) ? index : defaultIndex;
}

/**
uint safeIndex = GetSafeIndex(normalTextureIndex, DEFAULT_NORMAL_TEXTURE_INDEX);
Texture2D<float4> normalTexture = ResourceDescriptorHeap[safeIndex];

or Engine Must provide a valid index, you can query the default index and set the index correctly

if sampler index is invalid, use static sampler
*/
