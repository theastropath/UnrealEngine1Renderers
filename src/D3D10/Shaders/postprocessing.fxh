struct VS_INPUT_SIMPLE
{	
	float3 pos : POSITION;
	//float2 texCoords: TEXCOORD0;
};

struct PS_INPUT_SIMPLE
{
	float4 pos: SV_POSITION;
	//float2 texCoords: TEXCOORD0;
};

struct PS_OUTPUT_SIMPLE
{
	float4 color: SV_Target;
};

struct PS_OUTPUT_SIMPLE_R32
{
	float color: SV_Target;
};

PS_INPUT_SIMPLE VS_identity( VS_INPUT_SIMPLE input )
{
	PS_INPUT_SIMPLE output = (PS_INPUT_SIMPLE)0;
	output.pos = float4(input.pos,1);
	//output.texCoords = input.texCoords;
	return output;
}

/** Luminance weights, Rec.709. Shared so tone mapping and edge detection agree. */
static const float3 LUMINANCE_VECTOR = float3(0.2125f, 0.7154f, 0.0721f);

shared cbuffer ppcbuffer
{
	/** SetIntVector() writes four; .xy is used */
	uint4 viewPort;
	float elapsedTime; /**<Time because last frame*/
}

shared Texture2D inputTexture;


