/** Shader for world geometry */

#include "common.fxh"
#include "unrealpool.fxh"
#include "unreal_pom.fx"

#define DETAIL_MAX 3 //detail texture passes
#define NEAR_Z UNREAL_DETAIL_NEAR_Z //Shared with unreal_pom.fx, unrealpool.fxh
#define COMPENSATE_FF_MODULATE 2 //fixed function does 2*s*d
#define NUM_TEXTURE_PASSES 7

#if(REDUCE_BANDING!=0 && BGRA7_EXPAND==2)
	#define BGRA7_TO_8BIT (255.0f/127.0f)
#else
	#define BGRA7_TO_8BIT BGRA7_EXPAND
#endif


#if(USE_ONEX_BLENDING)
	#define LIGHTMAP_BLEND_SCALE 1
#else
	#define LIGHTMAP_BLEND_SCALE 2
#endif

#define NUM_TEXTURE_COORDS 5

#define PASS_LIGHT 0
#define PASS_DETAIL 1
#define PASS_FOG 2
#define PASS_MACRO 3
#define PASS_BUMP 4
#define PASS_HEIGHT 5

/**@name POM displacement scale, per layer */
//@{
#define POM_SCALE_HEIGHT 0.03
#define POM_SCALE_DETAIL 0.1
//@}

struct VS_INPUT
{	
	float3 pos : POSITION;
	float2 tex[NUM_TEXTURE_COORDS]: TEXCOORD0;	
	uint flags: BLENDINDICES0;
	uint texturePasses: BLENDINDICES1;
};

struct PS_INPUT
{	
	float4 pos : SV_POSITION;
	float4 tex01: TEXCOORD0; /**< Diffuse coordinate in .xy, lightmap in .zw */
	float4 tex23: TEXCOORD1; /**< Detail in .xy, fog map in .zw */
	float2 tex4: TEXCOORD2; /**< Macro coordinate */
	/** Poly flags in .x, texture passes in .y */
	nointerpolation uint2 flags: BLENDINDICES0;
	centroid float4 origPos: POSITION; //For detail texture gliches with MSAA
	centroid float2 texCentroid: TEXCOORD3; //For MSAA on masked textures
};

#define usesPass(mask,pass) (((mask)&(1u<<(pass)))!=0)


struct PS_OUTPUT2
{
	float4 color: SV_Target0;
	float4 color1: SV_Target1;
};

Texture2D textures[NUM_TEXTURE_PASSES-1];

BlendState bstate_Translucent_ComplexSurface //Simulates multi-pass light modulation
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = SRC1_COLOR ;
	BlendOp = ADD;
};



//--------------------------------------------------------------------------------------
//Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
	PS_INPUT output = (PS_INPUT)0;


	float4 projected=mul(float4(input.pos,1),projection);
	output.origPos = float4(input.pos,1);
	output.pos = projected;			
	
	output.tex01 = float4(input.tex[0],input.tex[1]);
	output.tex23 = float4(input.tex[2],input.tex[3]);
	output.tex4 = input.tex[4];
	output.texCentroid = input.tex[0];
	output.flags = uint2(input.flags,input.texturePasses);
	
	//d3d vs unreal coords
	output.pos.y =  -output.pos.y;
	output.origPos.y = -output.origPos.y;
	
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT2 PS( PS_INPUT input)
{
	PS_OUTPUT2 output;
	output.color=float4(1,1,1,1);
	output.color1=float4(1,1,1,1);

	float2 tex[NUM_TEXTURE_COORDS] = {input.tex01.xy,input.tex01.zw,input.tex23.xy,input.tex23.zw,input.tex4};
	float2 texCentroid = input.texCentroid;
	const uint flags = input.flags.x;
	const uint texturePasses = input.flags.y;

	float NearZ=NEAR_Z;			
	bool isNear = input.origPos.z < NearZ;
	
	

	#if(POM_ENABLED!=0)
	if(isNear)
	{
		float2 texBeforePOM = tex[0];
		if(usesPass(texturePasses,PASS_HEIGHT)) //Height map
		{	
			float2 texPom = parallaxOcclusionMap(input.origPos,tex[0],POM_SCALE_HEIGHT,textures[PASS_HEIGHT]);	
			tex[0] = lerp(texPom,tex[0],input.origPos.z/NearZ);
		}
		else if(usesPass(texturePasses,PASS_DETAIL)) //No height map, so POM the detail level
		{
			tex[2] = parallaxOcclusionMap(input.origPos,tex[2],POM_SCALE_DETAIL,textures[PASS_DETAIL]);									
		}

		texCentroid += tex[0] - texBeforePOM;
	}
	#endif
		
	float4 diffuse = texDiffuse.SampleBias(sam,tex[0],LODBIAS);
	float4 diffusePoint = texDiffuse.SampleBias(samPoint,texCentroid,LODBIAS); //Centroid sampling for AA
	output.color*=diffuseTexture(diffuse,diffusePoint,flags);
	#if(!USE_CLASSIC_LIGHTING)
	if(flags&PF_Unlit)
		output.color.rgb=increaseDynamicRange(output.color.rgb,flags);
	#endif


	float3 light=float3(1,1,1);
	if(usesPass(texturePasses,PASS_LIGHT)) //Light
	{
		#if(SEVEN_BIT_MAP_RECONSTRUCTION==1)
		light= sampleSevenBitMap(textures[PASS_LIGHT],tex[1],input.pos.xy).rgb;
		#else
		light= textures[PASS_LIGHT].SampleLevel(sam,tex[1],0).rgb;
		#endif

		light= saturate(light.bgr*BGRA7_TO_8BIT)*LIGHTMAP_BLEND_SCALE;

		#if(!USE_CLASSIC_LIGHTING)
		light=increaseDynamicRange(light,flags);
		#endif

		output.color.rgb *=light;
	}
	
	if(usesPass(texturePasses,PASS_DETAIL)) //Detail
	{
		float3 detail=float3(1,1,1);				
					
		int DetailMax = DETAIL_MAX;

		float DetailScale=1.0f; 
		
		while( isNear && DetailMax-- > 0 )			
		{															
			float3 detailSample =  textures[PASS_DETAIL].SampleLevel(sam,tex[2]*DetailScale,0).rgb;
			float detailAlpha    =  1-input.origPos.z/NearZ;			
			detail *=  (1-detailAlpha) + detailAlpha*detailSample*COMPENSATE_FF_MODULATE;
			
			DetailScale *= 4.223f;
			NearZ /= 4.223f;	
			isNear = input.origPos.z < NearZ;				
		}
		
		output.color.rgb *= detail;
	}
	
	float4 fogMap=float4(0,0,0,0);
	if(usesPass(texturePasses,PASS_FOG)) //Fog texture
	{		
		#if(SEVEN_BIT_MAP_RECONSTRUCTION==1)
		fogMap = sampleSevenBitMap(textures[PASS_FOG],tex[3],input.pos.xy);
		#else
		fogMap = textures[PASS_FOG].SampleLevel(sam,tex[3],0);				
		#endif
		fogMap.rgb = saturate(fogMap.bgr*BGRA7_TO_8BIT);
		fogMap.a = saturate(fogMap.a*BGRA7_TO_8BIT);
	}
	if(usesPass(texturePasses,PASS_MACRO)) //Macro
	{		
		//.rgb only.
		output.color.rgb *= textures[PASS_MACRO].Sample(sam,tex[4]).rgb*COMPENSATE_FF_MODULATE;
	}
	#if(BUMPMAPPING_ENABLED!=0)
		if(usesPass(texturePasses,PASS_BUMP)) //Bumpmap
		{		
			float3 bumpMap=textures[PASS_BUMP].Sample(sam,tex[0]).xyz;
			bumpMap=normalize(bumpMap*2-1);
			
			/* No light list is given. */
			const float3 lightVec = float3(1,1,1); //The look the option has always had.
			output.color.rgb *= saturate(dot(lightVec,bumpMap));
			
		}
	#endif
	
	//Simulates 3DFX multi-pass behaviour.

	/*
	The surface's own lightmap does not scale this; what shows through is lit by its surroundings.
	*/
	float3 surfaceBeforeFog = output.color.rgb;

	//Alternate behavior - results in darker shadows from volumetric fog, which hides lightmap issues
	//float3 destLight = saturate(saturate(1-surfaceBeforeFog/max(light.rgb,0.00001))*light.rgb);
	float3 destLight = saturate(1-surfaceBeforeFog/max(light.rgb,0.00001));

	//The fog layer attenuates the surface and what shows through it alike, as a separate highlighted
	//pass would. Alpha is left alone, the fog stage selecting the surface's own.
	output.color.rgb = surfaceBeforeFog*(1-fogMap.a) + fogMap.rgb;
	output.color1.rgb = destLight*(1-fogMap.a);

	return output;
}


//--------------------------------------------------------------------------------------
technique10 Render
{
	pass Standard
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
		
		SetRasterizerState(rstate_Default);             
	}

	/** As Standard, with the mover bias. */
	pass MoverBias
	{
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );

		SetRasterizerState(rstate_MoverBias);
	}
}
