
#ifndef C_GCLIP_H
#define C_GCLIP_H

#include <limits>
#include <deque>

class CGClip {
public:
	enum { NUM_CP = 6 };

public:
	typedef struct {
		float x;
		float y;
		float z;
	} vec3_t;

	typedef struct {
		vec3_t n;
		float d;
	} plane_t;

	typedef struct {
		unsigned int numPts;
		vec3_t pts[2];
	} cl_line_t;

	typedef struct {
		unsigned int numPts;
		vec3_t pts[3 + NUM_CP];
	} cl_tri_t;

public:
	CGClip();
	~CGClip();

public:
	void SetCpEnable(unsigned int cpNum, bool cpEnable);
	void SetCp(unsigned int cpNum, const float *pPlane);

	//Inline: as a .cpp function it only linked because its sole caller happened to
	//share that translation unit.
	inline float Dot3(const vec3_t &v1, const vec3_t &v2) {
		return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
	}

	void ClipLine(cl_line_t &clLine);
	void ClipTri(cl_tri_t &clTri);

	void SelectModeStart(void);
	void SelectModeEnd(void);

	bool CheckNewSelectHit(void);
	void ClearHitNameStack(void);
	void PushHitName(unsigned int hitName);
	void PopHitName(void);
	unsigned int GetHitNameStackSize(void);
	void GetHitNameStackValues(unsigned int *pDst, unsigned int dstSize);

	void SelectDrawLine(const vec3_t *pLnPts);
	void SelectDrawTri(const vec3_t *pTriPts);

private:
	unsigned int m_cpEnableBits;
	plane_t m_cp[NUM_CP];

	std::deque<unsigned int> m_hitNameStack;
	float m_selClosestDepth;
	bool m_selHit;
};

#endif
