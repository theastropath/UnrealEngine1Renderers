#include "../OpenGLDrv.h"
#include "../OpenGL.h"

int UOpenGLRenderDevice::dbgPrintf(const char *format, ...) {
	const unsigned int DBG_PRINT_BUF_SIZE = 1024;
	char dbgPrintBuf[DBG_PRINT_BUF_SIZE];
	va_list vaArgs;
	int iRet = 0;

	va_start(vaArgs, format);

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4996)
	iRet = vsnprintf(dbgPrintBuf, DBG_PRINT_BUF_SIZE, format, vaArgs);
	dbgPrintBuf[DBG_PRINT_BUF_SIZE - 1] = '\0';
#pragma warning(pop)

	TCHAR_CALL_OS(OutputDebugStringW(appFromAnsi(dbgPrintBuf)), OutputDebugStringA(dbgPrintBuf));
#endif

	va_end(vaArgs);

	return iRet;
}


void UOpenGLRenderDevice::LogTileOnce(const FTextureInfo &Info, DWORD polyFlags, DWORD blendPolyFlags) {
	const QWORD cacheID = (QWORD)Info.CacheID;
	for (DWORD u = 0; u < m_numLoggedTiles; u++) {
		if (m_loggedTileIds[u] == cacheID) {
			return;
		}
	}
	if (m_numLoggedTiles >= MAX_LOGGED_TILES) {
		return;
	}
	m_loggedTileIds[m_numLoggedTiles++] = cacheID;

	debugf(NAME_Log, TEXT("utglr: DrawTile id %08X%08X format %i palette %i marker %i PolyFlags %08X drawn %08X"),
		(DWORD)(cacheID >> 32), (DWORD)cacheID,
		(INT)Info.Format,
		(Info.Palette != NULL) ? 1 : 0,
		(Info.Palette && (Info.Palette[128].A != 255)) ? 1 : 0,
		polyFlags, blendPolyFlags);
}

void UOpenGLRenderDevice::DbgPrintInitParam(const char *pName, INT value) {
	dbgPrintf("utglr: %s = %d\n", pName, value);
	return;
}

void UOpenGLRenderDevice::DbgPrintInitParam(const char *pName, FLOAT value) {
	dbgPrintf("utglr: %s = %f\n", pName, value);
	return;
}

void UOpenGLRenderDevice::DbgPrintInitParam(const char *pName, BITFIELD value) {
	DbgPrintInitParam(pName, (INT)value);
	return;
}
