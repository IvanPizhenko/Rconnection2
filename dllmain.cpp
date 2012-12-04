#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>

int nWSAResult = 0;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  fdwReason,
                       LPVOID lpReserved
					 )
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			WSADATA dt;
			nWSAResult = WSAStartup(0x0101, &dt);
			return nWSAResult == 0 ? TRUE : FALSE;
		}
		case DLL_PROCESS_DETACH:
		{
			if (nWSAResult == 0)
				WSACleanup();
			break;
		}
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}

