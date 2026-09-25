#include "common.fxh"
#include "postprocessing.fxh"

#if(SAMPLES>1)

Texture2DMS<float4,SAMPLES> sceneBuffer;

float4 resolveScene(int2 coords)
{
	float4 color=(float4)0;
	[unroll] for(int i=0;i<SAMPLES;i++)
	{
		color += sceneBuffer.Load(coords,i);
	}
	return color/SAMPLES;
}

#else

Texture2D sceneBuffer;

float4 resolveScene(int2 coords)
{
	return sceneBuffer.Load(int3(coords,0));
}

#endif

PS_OUTPUT_SIMPLE PS_first(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;

	output.color=resolveScene((int2)input.pos.xy);

	return output;
}


technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_first() ) );
		SetRasterizerState(rstate_NoMSAA);
	}
}
