/** Accounts for the sample count */

/* Convert from C-style boolean to HLSL boolean */
#define USE_CLASSIC_LIGHTING (CLASSIC_LIGHTING!=0)
#define USE_ONEX_BLENDING (ONEX_BLENDING!=0)

/* SAMPLERS */

// Main sampler; follows TextureFiltering
SamplerState sam
{
	#if(TEXTURE_FILTERING==0)
	Filter = MIN_MAG_MIP_POINT;
	#elif(TEXTURE_FILTERING==1)
	Filter = MIN_MAG_MIP_LINEAR;
	#else
	Filter = ANISOTROPIC;
	MaxAnisotropy=NUM_ANISO;
	#endif
	AddressU = Wrap;
	AddressV = Wrap;
};

SamplerState samLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};


SamplerState samPoint
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap;
	AddressV = Wrap;
};

SamplerState samPointClamp
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
};

SamplerState samClamp
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

/* DITHER */

#if(REDUCE_BANDING!=0)
/** Interleaved gradient noise, unanimated. */
float ign(float2 p)
{
	return frac(52.9829189f*frac(dot(p,float2(0.06711056f,0.00583715f))));
}

/** Triangular PDF in (-1,1) from one uniform sample. */
float triangular(float u)
{
	float o = u*2.0f-1.0f;
	return sign(o)*(1.0f-sqrt(saturate(1.0f-abs(o))));
}
#endif

/* SEVEN BIT MAP RECONSTRUCTION */

#if(REDUCE_BANDING!=0 && TEXTURE_FILTERING!=0)
	#define SEVEN_BIT_MAP_RECONSTRUCTION 1
#else
	#define SEVEN_BIT_MAP_RECONSTRUCTION 0
#endif

#if(SEVEN_BIT_MAP_RECONSTRUCTION==1)

#define BICUBIC_TEXEL_REACH 2

/**
Reconstructs a lightmap or fog map the engine hands over at roughly one texel per 32 world units and
seven bits per channel, both of which read as banding, answered by a B-spline and a one-code dither.

\param tex Sampled at mip zero.
\param pixel Position on screen, for the dither.
\note Taps are clamped.
*/
float4 sampleSevenBitMap(Texture2D tex, float2 uv, float2 pixel)
{
	float width, height;
	tex.GetDimensions(width,height);
	const float2 size = float2(width,height);
	const float2 coord = uv*size-0.5f;
	const float2 base = floor(coord);
	const float2 f = coord-base;
	const float2 f2 = f*f;
	const float2 f3 = f2*f;
	//B-spline weights, summing to one
	const float2 w0 = (1.0f/6.0f)*(-f3+3.0f*f2-3.0f*f+1.0f);
	const float2 w1 = (1.0f/6.0f)*(3.0f*f3-6.0f*f2+4.0f);
	const float2 w2 = (1.0f/6.0f)*(-3.0f*f3+3.0f*f2+3.0f*f+1.0f);
	const float2 w3 = (1.0f/6.0f)*f3;
	const float2 g0 = w0+w1;
	const float2 g1 = w2+w3;
	const float2 tap0 = (base-0.5f+w1/g0)/size;
	const float2 tap1 = (base+1.5f+w3/g1)/size;
	const float4 row0 = lerp(tex.SampleLevel(samClamp,float2(tap0.x,tap0.y),0),
		tex.SampleLevel(samClamp,float2(tap1.x,tap0.y),0),g1.x);
	const float4 row1 = lerp(tex.SampleLevel(samClamp,float2(tap0.x,tap1.y),0),
		tex.SampleLevel(samClamp,float2(tap1.x,tap1.y),0),g1.x);
	const float4 reconstructed = lerp(row0,row1,g1.y);

	const float4 dithered = saturate(reconstructed+triangular(ign(pixel))/255.0f);
	const float brightest = max(reconstructed.r,max(reconstructed.g,reconstructed.b));

	return lerp(reconstructed,dithered,saturate(brightest*255.0f));
}

#endif

/** States */
RasterizerState rstate_Default
{
	CullMode = NONE;
	FillMode = SOLID;
	MultisampleEnable=TRUE;

};

RasterizerState rstate_NoMSAA
{
	CullMode = NONE;
	FillMode = SOLID;
	MultisampleEnable=FALSE;
	
};

/** Decals on a drawn plane. Depth is reversed. */
RasterizerState rstate_DecalBias
{
	CullMode = NONE;
	FillMode = SOLID;
	MultisampleEnable=TRUE;
	#if(DECAL_DEPTH_BIAS!=0)
	DepthBias = DECAL_DEPTH_BIAS;
	SlopeScaledDepthBias = 1.0;
	#endif
};

/** Movers, never cut against the level. */
RasterizerState rstate_MoverBias
{
	CullMode = NONE;
	FillMode = SOLID;
	MultisampleEnable=TRUE;
	DepthBias = -512;
	SlopeScaledDepthBias = -1.0;
};


/* MISC */

//Their own header, so neither side drifts.
#include "polyflags.fxh"
