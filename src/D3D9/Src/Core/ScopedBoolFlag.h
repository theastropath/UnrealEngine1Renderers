/*=============================================================================
	ScopedBoolFlag.h: a bool held for the length of a scope.
=============================================================================*/

#pragma once


//These regions call back into the ordinary draw path, where a fatal error throws.
class FScopedBoolFlag {
public:
	explicit FScopedBoolFlag(bool &flag) :
		m_pFlag(&flag) {
		*m_pFlag = true;
	}
	~FScopedBoolFlag(void) {
		*m_pFlag = false;
	}

private:
	bool *m_pFlag;

	FScopedBoolFlag(const FScopedBoolFlag &);
	FScopedBoolFlag &operator=(const FScopedBoolFlag &);
};
