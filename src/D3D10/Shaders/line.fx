
#include "common.fxh"
#include "unrealpool.fxh"

#define LINE_WIDTH 1.0f

struct VS_INPUT
{	
	float4 XYXY : POSITION0; //In pixels: X1, Y1, X2, Y2
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES0;
	uint isRect: BLENDINDICES1;
};

struct GS_INPUT
{	
	float4 XYXY: POSITION0;
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES0;
	uint isRect: BLENDINDICES1;
};

struct PS_INPUT
{	
	float4 pos : SV_POSITION;
	float4 color: COLOR0;
	uint flags: BLENDINDICES;
};

float2 screenToClip(float2 screen)
{
	return float2(-1.0f + screen.x/(0.5f*viewportWidth),
	               1.0f - screen.y/(0.5f*viewportHeight));
}

GS_INPUT VS( VS_INPUT input )
{
	GS_INPUT output = (GS_INPUT)input;

	output.color = unrealColor(input.color,input.flags);

	float4 projected = mul(float4(0,0,input.z,1),projection);
	output.z = projected.w>0 ? saturate(projected.z/projected.w) : 1.0f;

	return output;
}

[maxvertexcount(4)]
void GS( point GS_INPUT input[1], inout TriangleStream <PS_INPUT> triStream )
{
	PS_INPUT output = (PS_INPUT)0;

	output.pos.z = input[0].z;
	output.pos.w = 1;
	output.color = input[0].color;
	output.flags = input[0].flags;

	float2 a = input[0].XYXY.xy;
	float2 b = input[0].XYXY.zw;

	float2 corners[4];
	if(input[0].isRect!=0)
	{
		corners[0] = float2(a.x,a.y);
		corners[1] = float2(a.x,b.y);
		corners[2] = float2(b.x,a.y);
		corners[3] = float2(b.x,b.y);
	}
	else
	{
		//Zero length collapses to nothing.
		float2 direction = b-a;
		float length2 = dot(direction,direction);
		float2 perpendicular = (length2>1e-12f)
			? float2(-direction.y,direction.x)*rsqrt(length2)*(0.5f*LINE_WIDTH)
			: float2(0,0);

		corners[0] = a+perpendicular;
		corners[1] = a-perpendicular;
		corners[2] = b+perpendicular;
		corners[3] = b-perpendicular;
	}

	[unroll] for(int i=0;i<4;i++)
	{
		output.pos.xy = screenToClip(corners[i]);
		triStream.Append(output);
	}

	triStream.RestartStrip();
}

PS_OUTPUT PS( PS_INPUT input)
{
	PS_OUTPUT output;
	output.color = input.color;
	return output;
}


struct VS_INPUT_INSTANCED
{
	float2 corner: TEXCOORD7;
	float4 XYXY : POSITION0;
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES0;
	uint isRect: BLENDINDICES1;
};

PS_INPUT VS_Instanced( VS_INPUT_INSTANCED input )
{
	PS_INPUT output = (PS_INPUT)0;

	output.color = unrealColor(input.color,input.flags);
	output.flags = input.flags;

	float4 projected = mul(float4(0,0,input.z,1),projection);
	output.pos.z = projected.w>0 ? saturate(projected.z/projected.w) : 1.0f;
	output.pos.w = 1;

	float2 a = input.XYXY.xy;
	float2 b = input.XYXY.zw;
	float2 corner;

	if(input.isRect!=0)
	{
		corner = float2(input.corner.x!=0 ? b.x : a.x,
		                input.corner.y!=0 ? b.y : a.y);
	}
	else
	{
		float2 direction = b-a;
		float length2 = dot(direction,direction);
		float2 perpendicular = (length2>1e-12f)
			? float2(-direction.y,direction.x)*rsqrt(length2)*(0.5f*LINE_WIDTH)
			: float2(0,0);

		corner = lerp(a,b,input.corner.x) + perpendicular*(1.0f-2.0f*input.corner.y);
	}

	output.pos.xy = screenToClip(corner);
	return output;
}


technique10 Render
{
	pass Standard
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( CompileShader( gs_4_0, GS()) );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );

		SetRasterizerState(rstate_NoMSAA);
	}
}

technique10 RenderInstanced
{
	pass Standard
	{
		SetVertexShader( CompileShader( vs_4_0, VS_Instanced() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );

		SetRasterizerState(rstate_NoMSAA);
	}
}
