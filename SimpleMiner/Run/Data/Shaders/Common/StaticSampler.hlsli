#pragma once

SamplerState s_pointWrap        : register(s0);
SamplerState s_pointClamp       : register(s1);
SamplerState s_linearWrap       : register(s2);
SamplerState s_linearClamp      : register(s3);
SamplerState s_anisotropicWrap  : register(s4);
SamplerState s_anisotropicClamp : register(s5);
SamplerState s_pointMipLinearWrap : register(s6);
SamplerState s_pointMipLinearClamp : register(s7);
