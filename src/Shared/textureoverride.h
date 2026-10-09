/*=============================================================================
	textureoverride.h: turning a texture's name into the files that replace it.

	The D3D10 renderer established this layout and packs already exist for it, so
	the rules here are its rules: the same object name has to produce the same
	path in all three renderers or a pack would load on one and not the others.

	Paths are built as plain char. An Unreal object name is ASCII and the prefix
	is a literal, so the whole path is, which keeps this free of the three
	different TCHAR conventions in the tree.
=============================================================================*/

#ifndef UTGLR_TEXTUREOVERRIDE_H
#define UTGLR_TEXTUREOVERRIDE_H

#include <stddef.h>
#include <string.h>


//Relative to the game's System directory, which is the working directory.
#define TEXOVERRIDE_DIRECTORY "..\\Textures\\"

//Only object names carrying this prefix are considered. Lightmaps have none.
#define TEXOVERRIDE_NAME_PREFIX "Texture "

#define TEXOVERRIDE_MAX_PATH 512


enum ETexOverrideKind {
	TEXOVERRIDE_DIFFUSE,
	TEXOVERRIDE_DETAIL
};

/*
Marks a cache id as standing for a texture's detail override rather than the
texture itself.

The engine builds a cache id from an object index in the upper word and a type
in the low byte, and never sets the top bit, so this cannot collide with one.
Setting it also guarantees a non zero upper half, which keeps these entries in
the utglr cache's 64 bit tree rather than the 32 bit one, where only the low
half of the key is compared and two overrides could alias.
*/
#define TEXOVERRIDE_DETAIL_CACHEID_BIT 0x8000000000000000ULL


/**
Build the override path for a texture.

\param pFullName The object's full name, as UObject::GetFullName returns it.
\param kind Which of the texture's files is wanted.
\param pOutPath Receives the path. Untouched unless this returns true.
\param outBytes Capacity of pOutPath, including the terminator.
\return false when the name is not a texture's or the path will not fit, both of
	which mean there is nothing to look for rather than that something is wrong.
*/
static inline bool TexOverrideBuildPath(const char *pFullName, ETexOverrideKind kind,
	char *pOutPath, size_t outBytes) {
	if ((pFullName == NULL) || (pOutPath == NULL) || (outBytes == 0)) {
		return false;
	}

	const size_t prefixLen = sizeof(TEXOVERRIDE_NAME_PREFIX) - 1;
	if (strncmp(pFullName, TEXOVERRIDE_NAME_PREFIX, prefixLen) != 0) {
		return false;
	}
	const char *pName = pFullName + prefixLen;

	const char *pSuffix = (kind == TEXOVERRIDE_DETAIL) ? ".detail.dds" : ".dds";

	const size_t dirLen = sizeof(TEXOVERRIDE_DIRECTORY) - 1;
	const size_t nameLen = strlen(pName);
	const size_t suffixLen = strlen(pSuffix);

	//Checked before anything is written, so a long name cannot leave a half built path.
	if ((dirLen + nameLen + suffixLen + 1) > outBytes) {
		return false;
	}
	if (nameLen == 0) {
		return false;
	}

	memcpy(pOutPath, TEXOVERRIDE_DIRECTORY, dirLen);
	memcpy(pOutPath + dirLen, pName, nameLen);

	/*
	Package.Group.Name becomes Package\Group\Name. The scan starts past the two
	leading dots of the directory prefix, which are a path and not separators;
	the suffix is appended afterwards so its own dot survives.
	*/
	for (size_t i = 2; i < (dirLen + nameLen); i++) {
		if (pOutPath[i] == '.') {
			pOutPath[i] = '\\';
		}
	}

	memcpy(pOutPath + dirLen + nameLen, pSuffix, suffixLen);
	pOutPath[dirLen + nameLen + suffixLen] = '\0';
	return true;
}

/**
Build the path of the flags sidecar belonging to an override.

\note The sidecar sits beside the file with its own extension appended, so a
	diffuse override at Name.dds is described by Name.dds.flags. Easy to get
	wrong by hand, and it is the name D3D10 has always written.
*/
static inline bool TexOverrideBuildFlagsPath(const char *pDDSPath, char *pOutPath, size_t outBytes) {
	if ((pDDSPath == NULL) || (pOutPath == NULL)) {
		return false;
	}

	const char *pSuffix = ".flags";
	const size_t pathLen = strlen(pDDSPath);
	const size_t suffixLen = strlen(pSuffix);
	if ((pathLen + suffixLen + 1) > outBytes) {
		return false;
	}

	memcpy(pOutPath, pDDSPath, pathLen);
	memcpy(pOutPath + pathLen, pSuffix, suffixLen);
	pOutPath[pathLen + suffixLen] = '\0';
	return true;
}

#endif //UTGLR_TEXTUREOVERRIDE_H
