/**
Shader for sprites.

Two ways to get from one record per tile to a quad, Render expanding a point in a geometry shader and
RenderInstanced drawing a unit quad once per tile with the record supplied per instance, the choice
being fixed in Shader_Tile when the shader is built.
*/

#include "common.fxh"
#include "unrealpool.fxh"

struct VS_INPUT
{	
	float4 XYWH : POSITION0; //In pixels: X, Y, width, height
	float4 UVWH: POSITION1; //Texture U, V, width, height
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES;
};

struct GS_INPUT
{	
	float4 XYWH: POSITION0;
	float4 UVWH: POSITION1;
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES;
};


struct PS_INPUT
{	
	float4 pos : SV_POSITION;
	float4 color: COLOR0;
	float2 tex: TEXCOORD0;
	uint flags: BLENDINDICES;
};

//--------------------------------------------------------------------------------------
//Vertex Shader
//--------------------------------------------------------------------------------------
GS_INPUT VS( VS_INPUT input )
{
	GS_INPUT output = (GS_INPUT)input;

	output.XYWH.xz/=0.5*viewportWidth;
	output.XYWH.yw/=-0.5*viewportHeight;


	output.color = unrealColor(input.color,input.flags);

	float4 projected=mul(float4(1,1,output.z,1),projection);
	//Saturating clamps tiles outside the frustum to the near or far plane, and at or behind the eye the
	//divide changes sign, which is why the branch is explicit.
	output.z = projected.w>0 ? saturate(projected.z/projected.w) : 1.0f;

	return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GS( point GS_INPUT input[1], inout TriangleStream <PS_INPUT> triStream )
{
	PS_INPUT output = (PS_INPUT)0;
	
	output.pos.z = input[0].z;
	output.pos.w = 1;
	output.color = input[0].color;
	output.flags = input[0].flags;

	

	float4 pos;
	pos.x = -1+input[0].XYWH.x;
	pos.y = pos.x +input[0].XYWH.z;
	pos.z =  1+input[0].XYWH.y;
	pos.w =  pos.z + input[0].XYWH.w;

	float4 tex;
	tex.x = input[0].UVWH.x;
	tex.y = tex.x + input[0].UVWH.z;	
	tex.z = input[0].UVWH.y;
	tex.w = tex.z + input[0].UVWH.w;
	
	output.pos.xy = pos.xz;
	output.tex.xy = tex.xz;
	triStream.Append(output);
	
	output.pos.xy = pos.xw;
	output.tex.xy = tex.xw;
	triStream.Append(output);

	//Right top. One depth for all four.
	output.pos.xy = pos.yz;
	output.tex.xy = tex.yz;
	triStream.Append(output);
	
	output.pos.xy = pos.yw;
	output.tex.xy = tex.yw;
	triStream.Append(output);
	
	triStream.RestartStrip();
}



//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS( PS_INPUT input)
{
	PS_OUTPUT output;
	 
	output.color= input.color;	
	float4 diffuse = texDiffuse.SampleBias(sam,input.tex,LODBIAS);
	float4 diffusePoint = texDiffuse.SampleBias(samPoint,input.tex,LODBIAS);
		
	output.color*=diffuseTexture(diffuse,diffusePoint,input.flags);

	return output;
}



//--------------------------------------------------------------------------------------
// Instanced vertex shader
//
// Slot 0 the corner, slot 1 the tile record.
//--------------------------------------------------------------------------------------
struct VS_INPUT_INSTANCED
{
	float2 corner: TEXCOORD7;	//Per vertex
	float4 XYWH : POSITION0;	//Per instance, from here down
	float4 UVWH: POSITION1;
	float4 color: COLOR0;
	float z: PSIZE0;
	uint flags: BLENDINDICES;
};

PS_INPUT VS_Instanced( VS_INPUT_INSTANCED input )
{
	PS_INPUT output = (PS_INPUT)0;

	float4 xywh = input.XYWH;
	xywh.xz /= 0.5*viewportWidth;
	xywh.yw /= -0.5*viewportHeight;

	output.color = unrealColor(input.color,input.flags);
	output.flags = input.flags;

	float4 projected = mul(float4(1,1,input.z,1),projection);
	output.pos.z = projected.w>0 ? saturate(projected.z/projected.w) : 1.0f;
	output.pos.w = 1;

	output.pos.x = -1 + xywh.x + input.corner.x*xywh.z;
	output.pos.y =  1 + xywh.y + input.corner.y*xywh.w;

	output.tex.x = input.UVWH.x + input.corner.x*input.UVWH.z;
	output.tex.y = input.UVWH.y + input.corner.y*input.UVWH.w;

	return output;
}


//--------------------------------------------------------------------------------------
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