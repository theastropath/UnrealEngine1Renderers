
#include "common.fxh"
#include "postprocessing.fxh"

#define EDGE_THRESHOLD 0.125f
#define EDGE_THRESHOLD_MIN 0.0312f
#define EDGE_SPAN 2.0f

float luminance(float3 color)
{
	return dot(color,LUMINANCE_VECTOR);
}

float3 tap(int2 pixel, int2 offset)
{
	const int2 last = (int2)viewPort.xy-1;
	return inputTexture.Load(int3(clamp(pixel+offset,int2(0,0),last),0)).rgb;
}

PS_OUTPUT_SIMPLE PS_postAA(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;

	const float2 texelSize = 1.0f/float2(viewPort.xy);
	const float2 uv = input.pos.xy*texelSize;
	const int2 pixel = (int2)input.pos.xy;

	float3 rgbM  = tap(pixel,int2( 0, 0));
	float3 rgbNW = tap(pixel,int2(-1,-1));
	float3 rgbNE = tap(pixel,int2( 1,-1));
	float3 rgbSW = tap(pixel,int2(-1, 1));
	float3 rgbSE = tap(pixel,int2( 1, 1));

	float lumaM  = luminance(rgbM);
	float lumaNW = luminance(rgbNW);
	float lumaNE = luminance(rgbNE);
	float lumaSW = luminance(rgbSW);
	float lumaSE = luminance(rgbSE);

	float lumaMin = min(lumaM,min(min(lumaNW,lumaNE),min(lumaSW,lumaSE)));
	float lumaMax = max(lumaM,max(max(lumaNW,lumaNE),max(lumaSW,lumaSE)));
	float range = lumaMax-lumaMin;

	if(range < max(EDGE_THRESHOLD_MIN,lumaMax*EDGE_THRESHOLD))
	{
		output.color = float4(rgbM,1);
		return output;
	}

	float2 direction;
	direction.x = -((lumaNW+lumaNE)-(lumaSW+lumaSE));
	direction.y =  ((lumaNW+lumaSW)-(lumaNE+lumaSE));
	float longest = max(abs(direction.x),abs(direction.y));
	direction = clamp(direction/max(longest,1e-6f),-1.0f,1.0f)*EDGE_SPAN*texelSize;

	float3 rgbInner = 0.5f*(inputTexture.SampleLevel(samClamp,uv+direction*(1.0f/3.0f-0.5f),0).rgb
	                      + inputTexture.SampleLevel(samClamp,uv+direction*(2.0f/3.0f-0.5f),0).rgb);
	float3 rgbOuter = rgbInner*0.5f + 0.25f*(inputTexture.SampleLevel(samClamp,uv-direction*0.5f,0).rgb
	                                       + inputTexture.SampleLevel(samClamp,uv+direction*0.5f,0).rgb);

	//Outside the range, it blurs a feature.
	float lumaOuter = luminance(rgbOuter);
	output.color = float4((lumaOuter<lumaMin || lumaOuter>lumaMax) ? rgbInner : rgbOuter,1);
	return output;
}


technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_postAA() ) );

		SetRasterizerState(rstate_NoMSAA);
	}
}
