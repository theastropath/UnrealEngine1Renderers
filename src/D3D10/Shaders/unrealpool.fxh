/* Shared by the geometry shaders */

#include "polyflags.fxh"


// Detail and POM fade out by here, in world units.
#define UNREAL_DETAIL_NEAR_Z 380.0f


struct PS_OUTPUT
{
	float4 color: SV_Target0;
};

/* CONSTANT BUFFERS */
shared cbuffer PerScene
{
	matrix projection;
	float viewportHeight;
	float viewportWidth;
}


shared Texture2D texDiffuse;


float4 unrealColor(float4 color, uint flags)
{		
	if(flags&PF_Modulated) //Modulated not influenced by color
		return float4(1,1,1,1);
	
	float4 result = clamp(color,0,1); //Rune sends >1, which wrecks runestone particles

	return result;
}

/** Calculate fog color */
float4 unrealVertexFog(float4 fog, uint flags)
{
	//From the OpenGL renderer.
	if((flags & (PF_RenderFog|PF_Translucent|PF_Modulated|PF_AlphaBlend))==PF_RenderFog)
		return fog;
	
	return float4(0,0,0,0);
}

/** Increase dynamic range for HDR */
float3 increaseDynamicRange(float3 color,uint flags)
{
	float3 output=color;
	if(!(flags&PF_Modulated))
	{
		output.rgb=pow(abs(output.rgb)*1.5f,1.5);
	}
	return output;
}

/** Darken to match HDR environments */
float3 increaseDynamicRange_DarkenOnly(float3 color,uint flags)
{
	float3 output=color;
	if(!(flags&PF_Modulated))
	{
		output.rgb=pow(abs(output.rgb),1.5);
	}
	return output;
}

/** Handle diffuse texturing/alpha test */
float4 diffuseTexture(float4 diffuse, float4 diffusePoint, uint flags)
{
	//Alpha test. Point sampled for seams.
	if(flags&PF_Masked && !(flags&(PF_Translucent|PF_AlphaBlend|PF_Invisible))) //External textures can carry alpha
	{
		/*
		Alpha to coverage only handles the cut-out under bstate_Masked. PF_Masked|PF_Modulated and
		PF_Masked|PF_Highlighted go elsewhere, and with coverage on they lost this clip() and came out black.
		*/
		#if(ALPHA_TO_COVERAGE_ENABLED)
		if(flags&(PF_Modulated|PF_Highlighted))
		#endif
		{
			clip(diffusePoint.a-0.5f);
			clip(diffuse.a-0.5f);
		}
	}
	
	if(flags&PF_NoSmooth)
	{
		diffuse = diffusePoint;
	}

	return diffuse;
}