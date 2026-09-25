/** \file d3d.h */

#pragma once
#include <D3D10.h>
#include <D3DX10.h>
#include "../Geometry/VertexFormats.h"
#include "../Shaders/Shader.h"

class DeferredGeometry;

class D3D {
private:
	static int createRenderTargetViews();
	static int findAALevel();
	static bool initShaders();
	static int resizeSwapChain(int X, int Y, bool fullScreen);

public:
	/** \note Must stay contiguous. */
	enum ShaderName { SHADER_TILE,
		SHADER_GOURAUDPOLYGON,
		SHADER_COMPLEXSURFACE,
		SHADER_FOGSURFACE,
		SHADER_LINE,
		SHADER_FIRSTPASS,
		SHADER_HDR,
		SHADER_POSTAA,
		SHADER_FINALPASS,
		DUMMY_NUM_SHADERS };


	//enum Polyflag {};
	enum Filtering { FILTER_POINT,
		FILTER_LINEAR,
		FILTER_ANISOTROPIC,
		DUMMY_NUM_FILTERING };

	/** Options. */
	struct Options {
		int samples;
		int VSync;
		int aniso;
		int LODBias;
		int POM; /**< Parallax occlusion mapping */
		int bumpMapping;
		int alphaToCoverage;
		int classicLighting;
		int oneXBlending; /**< One blend pass. */
		int filtering;
		int postAA; /**< Edge directed. */
		int reduceBanding; /**< 10 bit composite, dithered down. */
		int clipboardScreenshots; /**< On Print Screen. */
		int decalDepthBias; /**< Depth-buffer steps. Zero disables it. */
		/**
		Draws tiles, lines and points as instances of a unit quad. A geometry shader costs more.
		\note Decides an input layout, so it is compiled in.
		*/
		int instancing;
	};

	/** Whether a change forces a rebuild. */
	static bool optionsNeedRebuild(const Options &a, const Options &b);

	/** Otherwise process-wide statics. */
	//@{
	struct Context;
	static Context *createContext();
	static void destroyContext(Context *context);
	static void makeCurrent(Context *context);
	//@}
	/**@name API initialization/upkeep */
	//@{
	static int init(HWND hwnd, const D3D::Options &createOptions);
	static void uninit();
	static int resize(int X, int Y, bool fullScreen);
	static ID3D10Device *getDevice();
	static DXGI_FORMAT getDrawPassFormat();
	static DXGI_FORMAT getCompositeFormat();
	static bool isDeviceLost();
	static void setRuntimeOptions(const Options &newOptions);
	//@}

	/**@name Setup/clear frame */
	//@{
	static void newFrame(float time);
	//@}

	/**@name Prepare and render buffers */
	//@{
	static void render();
	static void postprocess();
	static void present();
	static DeferredGeometry *getDeferred();
	//@}


	/**@name Set state (projection, flags) */
	//@{
	static void switchToShader(int index);
	static Shader *getShader(int index);
	static void setViewPort(int X, int Y, int left, int top);
	/**
	Binds output-merger targets and skips redundant rebinds, null unbinding, cached here because the
	shaders re-bind on every switch and asking the device what is already bound is a costly round trip
	to pay on each of them.
	*/
	static void setRenderTargets(ID3D10RenderTargetView *renderTargetView, ID3D10DepthStencilView *depthStencilView);
	/** Null if none. */
	static ID3D10RenderTargetView *getRenderTarget();
	//@}

	/**@name Misc */
	//@{
	static void flash(Vec4 &color);
	static unsigned int getBufferFlushCount();
	static unsigned int getMoverBiasSwitchCount();
	static void countMoverBiasSwitch();
	static TCHAR *getModes();
	static void getScreenshot(Vec4_byte *buf, int width, int height);
	static void setBrightness(float brightness);
	//@}
};
