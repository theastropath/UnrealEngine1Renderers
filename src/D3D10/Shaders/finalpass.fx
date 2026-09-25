#include "common.fxh"
#include "postprocessing.fxh"

#define GAMMA_BRIGHTNESS_SCALE 2.5f

cbuffer finalConstants
{
	float brightness;

	//.rgb is added, .a is the engine's FlashScale.
	float4 flash;
}

PS_OUTPUT_SIMPLE PS_final(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;
				
	int3 coords = int3(input.pos.xy,0);		
	output.color=inputTexture.Load(coords);

	//Flash, before the gamma curve as it was when the first pass applied it, the multiply being the
	//INV_SRC_ALPHA half of the reference renderers' PF_Highlighted blend, without which a flash could
	//only ever add light and never darken the scene the way the originals do.
	output.color.rgb=output.color.rgb*flash.a+flash.rgb;
	
	//Apply brightness
	float gamma = GAMMA_BRIGHTNESS_SCALE*brightness;
	output.color.rgb=pow(abs(output.color.rgb),1.0f/gamma);

	#if(REDUCE_BANDING!=0)
	const float2 pixel = input.pos.xy;
	const float3 dithered = output.color.rgb + triangular(ign(pixel))/255.0f;
	output.color.rgb = lerp(output.color.rgb,dithered,saturate(max(output.color.r,max(output.color.g,output.color.b))*255.0f));
	#endif

	return output;
}

technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_final() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}