
#include "common.fxh"
#include "unrealpool.fxh"

struct VS_INPUT
{	
	float3 pos : POSITION;
	float4 color: COLOR0;
	float4 fog: COLOR1;
	float2 tex: TEXCOORD0;
	uint flags: BLENDINDICES;
};


struct PS_INPUT
{	
	float4 pos : SV_POSITION;
	centroid float4 color: COLOR0;
	centroid float4 fog: COLOR1;
	float2 tex: TEXCOORD0;
	uint flags: BLENDINDICES;
	float viewDepth: TEXCOORD1;
};

cbuffer Fog
{
	float fogDist;
	float4 fogColor;
}


PS_INPUT VS( VS_INPUT input )
{
	PS_INPUT output = (PS_INPUT)0;

	float4 projected=mul(float4(input.pos,1),projection);
	output.pos = projected;
	output.viewDepth = input.pos.z;
	
	
	output.color = unrealColor(input.color,input.flags);
	
	
	output.fog = unrealVertexFog(input.fog, input.flags);
	
	output.tex = input.tex;
	output.flags = input.flags;
	
	output.pos.y =  -output.pos.y;
	return output;
}



PS_OUTPUT PS( PS_INPUT input)
{
	PS_OUTPUT output;
	 
	float4 fog = input.fog;
	output.color= input.color;
		
	float4 diffuse = texDiffuse.SampleBias(sam,input.tex,LODBIAS);
	float4 diffusePoint = texDiffuse.SampleBias(samPoint,input.tex,LODBIAS);
	output.color*=diffuseTexture(diffuse,diffusePoint,input.flags);
	
	output.color+=fog;
	
	if(fogDist>0)
		output.color = lerp(output.color,fogColor,saturate(input.viewDepth/fogDist));

	//Compensate for darker HDR, without glowing white coats
	#if(!USE_CLASSIC_LIGHTING)
		output.color.rgb=increaseDynamicRange_DarkenOnly(output.color.rgb,input.flags);
	#endif
	return output;
}


technique10 Render
{
	pass Standard
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
		
		SetRasterizerState(rstate_Default);             
	}

	pass DecalBias
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );

		SetRasterizerState(rstate_DecalBias);
	}
}