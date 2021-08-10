#ifndef WE_MAIN_H
#define WE_MAIN_H
#include <core.h>
#include <vector.h>

Color4 DefaultSelectionColor = {20, 200, 20, 100};
typedef struct _WED {
	Client *client;
	SVec start, end;
} *WEClientData;
#endif // WE_MAIN_H
