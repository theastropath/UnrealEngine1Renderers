#include "common.fxh"
#include "postprocessing.fxh"

//LUMINANCE_VECTOR is shared with postaa.fx; see postprocessing.fxh
static const float3 BLUE_SHIFT_VECTOR = float3(1.05f, 0.97f, 1.27f); 
static const float  MIDDLE_GRAY = 0.07f;
static const float  LUM_WHITE = 1.5f;
static const float  BRIGHT_THRESHOLD = 0.6f;

/**
Range the scene may occupy going into tone mapping. Nothing on the way into the floating point scene
buffer is clamped, and a single infinity or NaN would latch into the adapted luminance permanently,
as the adaptation feeds back on its own previous value.
*/
static const float MAX_SCENE_COLOR = 64.0f;
/** Adapted luminance is divided by, so it must stay clear of zero. */
static const float MIN_ADAPTED_LUMINANCE = 0.0001f;

//max() before min(): a NaN loses the comparison and so becomes 0 instead of staying NaN.
float3 clampScene(float3 color)
{
	return min(max(color,0.0f),MAX_SCENE_COLOR);
}

Texture2D luminanceTex;
Texture2D bloomTex;

// Gaussian filter with 15 weights, sigma = 3.0
static const float blurWeights[15] = {
	0.00884695, 0.01821591, 0.03356240, 0.05533504, 0.08163802,
	0.10777793, 0.12732458, 0.13459834, 0.12732458, 0.10777793,
	0.08163802, 0.05533504, 0.03356240, 0.01821591, 0.00884695
};

/**
Convert source texture to luminance by taking 3x3 averages
*/
PS_OUTPUT_SIMPLE_R32 PS_sourceToLum(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE_R32 output = (PS_OUTPUT_SIMPLE_R32)0;
				
	float2 coords = input.pos.xy/viewPort.xy;
	
	float4 vColor = 0.0f;
    float  fAvg = 0.0f;
    
    [unroll] for( int y = -1; y < 2; y++ )
    {
        [unroll] for( int x = -1; x < 2; x++ )
        {
            // Compute the sum of color values
            vColor = inputTexture.Sample(samPointClamp,coords,int2(x,y)); 
            fAvg += dot( clampScene(vColor.rgb), LUMINANCE_VECTOR );
        }
    }
    
    fAvg /= 9;

    output.color = fAvg;
	
	return output;
}

/**
Downscale luminance texture 3x3 times
*/
PS_OUTPUT_SIMPLE_R32 PS_downscaleLum(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE_R32 output = (PS_OUTPUT_SIMPLE_R32)0;
				
	float2 coords = input.pos.xy/viewPort.xy;
	
    [unroll] for( int y = -1; y < 2; y++ )
    {
        [unroll] for( int x = -1; x < 2; x++ )
        {
            // Compute the sum of color values
             output.color += inputTexture.Sample(samPointClamp,coords,int2(x,y)).r; 
        }
    }
    
    output.color /= 9;
	return output;
}

/**
Luminance adaptation
*/
PS_OUTPUT_SIMPLE_R32 PS_calculateAdaptedLum(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE_R32 output = (PS_OUTPUT_SIMPLE_R32)0;
	float fAdaptedLum = inputTexture.Sample(samPointClamp, float2(0, 0) ).r;
	float fCurrentLum = luminanceTex.Sample(samPointClamp, float2(0, 0) ).r;

	//Feeds back on its own previous value every frame, so a single bad value would be carried along
	//indefinitely.
	fAdaptedLum = min(max(fAdaptedLum,MIN_ADAPTED_LUMINANCE),MAX_SCENE_COLOR);
	fCurrentLum = min(max(fCurrentLum,MIN_ADAPTED_LUMINANCE),MAX_SCENE_COLOR);
	
	// The user's adapted luminance level is simulated by closing the gap between
	// adapted luminance and current luminance by .5% every frame, based on a
	// 30 fps rate. This is not an accurate model of human adaptation, which can
	// take longer than half an hour.
	float fNewAdaptation = fAdaptedLum + (fCurrentLum - fAdaptedLum) * ( 1 - pow( 0.995f, 30 * elapsedTime ) );
	output.color = min(max(fNewAdaptation,MIN_ADAPTED_LUMINANCE),MAX_SCENE_COLOR);
	
	return output;
}

/**
Bright pass
*/
PS_OUTPUT_SIMPLE PS_brightPass(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;
				
	float2 coords = input.pos.xy/viewPort.xy;
	
    float3 vColor = 0.0f;    
    float  fLum = luminanceTex.Sample(samPointClamp, float2(0, 0) ).r;
       
    [unroll] for( int y = -1; y < 2; y++ ) 
    {
        [unroll] for( int x = -1; x < 2; x++ )
        {
            // Compute the sum of color values
            float4 vSample = inputTexture.Sample( samPointClamp, coords, int2(x,y) );                       
            vColor += clampScene(vSample.rgb);
        }
    }
    
    // Divide the sum to complete the average
    vColor /= 9;
 
    // Bright pass and tone mapping
    vColor = max( 0.0f, vColor - BRIGHT_THRESHOLD );
    vColor *= MIDDLE_GRAY / (fLum + 0.001f);
    vColor *= (1.0f + vColor/LUM_WHITE);
    vColor /= (1.0f + vColor);
    
    output.color = float4(vColor, 1.0f);
	return output;
}

/**
Gaussian blur
*/
float4 gaussianBlur(Texture2D tex, float2 coords, bool horiz)
{
	float4 result=0;
	float4 vColor;

	[unroll] for( int iSample = 0; iSample < 15; iSample++ )
	{
		// Sample from adjacent points
		int2 offset=int2(0,iSample-7);
		if(horiz)
			offset.xy=offset.yx;
		vColor = tex.Sample( samPointClamp, coords, offset);
        
		result += vColor*blurWeights[iSample];
	}
	return result;
}

PS_OUTPUT_SIMPLE PS_blur_horiz(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;
	float2 coords = input.pos.xy/viewPort.xy;
	output.color = gaussianBlur(inputTexture,coords,true);
	return output;
}

PS_OUTPUT_SIMPLE PS_blur_vert(PS_INPUT_SIMPLE input)
{
	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;
	float2 coords = input.pos.xy/viewPort.xy;
	output.color = gaussianBlur(inputTexture,coords,false);
	return output;
}



PS_OUTPUT_SIMPLE PS_final(PS_INPUT_SIMPLE input)
{

	

	PS_OUTPUT_SIMPLE output = (PS_OUTPUT_SIMPLE)0;
	float2 coords = input.pos.xy/viewPort.xy;
	output.color = inputTexture.Sample( samPointClamp, coords);
	output.color.rgb = clampScene(output.color.rgb);
    float vLum = luminanceTex.Sample( samPointClamp, float2(0,0) ).r;
    float4 vBloom = bloomTex.Sample( samClamp, coords );
    
    vLum = min(max(vLum,MIN_ADAPTED_LUMINANCE),MAX_SCENE_COLOR);

	// Define a linear blending from -1.5 to 2.6 (log scale) which
	// determines the lerp amount for blue shift
    float fBlueShiftCoefficient = 1.0f - (vLum + 1.5)/1.1;
    fBlueShiftCoefficient = saturate(fBlueShiftCoefficient);

	// Lerp between current color and blue, desaturated copy
    float3 vRodColor = dot( (float3)output.color, LUMINANCE_VECTOR ) * BLUE_SHIFT_VECTOR;
    output.color.rgb = lerp( (float3)output.color, vRodColor, fBlueShiftCoefficient );

    // Tone mapping
    output.color.rgb *= MIDDLE_GRAY / (vLum + 0.001f);
    output.color.rgb *= (1.0f + output.color.rgb/LUM_WHITE);
    output.color.rgb /= (1.0f + output.color.rgb);
    
    output.color.rgb += 0.6f * vBloom.rgb;
    // Tone mapping plus bloom can still exceed the displayable range
    output.color.rgb = saturate(output.color.rgb);
    output.color.a = 1.0f;
    
	return output;
}

/////////////////////////////////////////////////////////////
//TECHNIQUES
/////////////////////////////////////////////////////////////
technique10 sourceToLum
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_sourceToLum() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

technique10 downscaleLum
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_downscaleLum() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

technique10 brightPass
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_brightPass() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

technique10 blur
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_blur_horiz() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
	pass P1
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_blur_vert() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

technique10 finalPass
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_final() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

technique10 adaptiveLum
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_4_0, VS_identity() ) );	
		SetGeometryShader( 0 );
		SetPixelShader( CompileShader( ps_4_0, PS_calculateAdaptedLum() ) );
		SetRasterizerState(rstate_NoMSAA);    	
	}
}

