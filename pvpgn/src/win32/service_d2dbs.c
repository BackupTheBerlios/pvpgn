#include <windows.h>

char serviceLongName[] = "d2dbs service";
char serviceName[] = "d2dbs";
char serviceDescription[] = "Diablo 2 database server";

extern int g_ServiceStatus;
extern int main(int argc, char *argv[]);

SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle = 0;

void Win32_ServiceInstall()
{
	SERVICE_DESCRIPTION sdBuf; 
	SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);

	if (serviceControlManager)
	{
		char path[_MAX_PATH + 10];
		if (GetModuleFileName( 0, path, sizeof(path)/sizeof(path[0]) ) > 0)
		{
			SC_HANDLE service;
			strcat(path, " --service");
			service = CreateService(serviceControlManager,
							serviceName, serviceLongName,
							SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
							SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, path,
							0, 0, 0, 0, 0);
			if (service)
			{
				sdBuf.lpDescription = serviceDescription;
				ChangeServiceConfig2(  
										service,                // handle to service  
										SERVICE_CONFIG_DESCRIPTION, // change: description  
										&sdBuf);
				CloseServiceHandle(service);
			}
		}
		CloseServiceHandle(serviceControlManager);
	}
}

void Win32_ServiceUninstall()
{
	SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);

	if (serviceControlManager)
	{
		SC_HANDLE service = OpenService(serviceControlManager,
			serviceName, SERVICE_QUERY_STATUS | DELETE);
		if (service)
		{
			SERVICE_STATUS serviceStatus;
			if (QueryServiceStatus(service, &serviceStatus))
			{
				if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
					DeleteService(service);
			}
			//DeleteService(service);

			CloseServiceHandle(service);
		}

		CloseServiceHandle(serviceControlManager);
	}

}

void WINAPI ServiceControlHandler(DWORD controlCode)
{
	switch (controlCode)
	{
		case SERVICE_CONTROL_INTERROGATE:
			break;

		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);

			g_ServiceStatus = 0;
			return;

		case SERVICE_CONTROL_PAUSE:
			g_ServiceStatus = 2;
			serviceStatus.dwCurrentState = SERVICE_PAUSED;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);
			break;

		case SERVICE_CONTROL_CONTINUE:
			serviceStatus.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);
			g_ServiceStatus = 1;
			break;

		default:
			if ( controlCode >= 128 && controlCode <= 255 )
				// user defined control code
				break;
			else
				// unrecognized control code
				break;
	}

	SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

void WINAPI ServiceMain(DWORD argc, char *argv[])
{
	// initialise service status
	serviceStatus.dwServiceType = SERVICE_WIN32;
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | 
        SERVICE_ACCEPT_PAUSE_CONTINUE; 
	serviceStatus.dwWin32ExitCode = NO_ERROR;
	serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
	serviceStatus.dwCheckPoint = 0;
	serviceStatus.dwWaitHint = 0;

	serviceStatusHandle = RegisterServiceCtrlHandler(serviceName, ServiceControlHandler);

	if ( serviceStatusHandle )
	{
		char path[_MAX_PATH + 1];
		int i, last_slash = 0;

		GetModuleFileName(0, path, sizeof(path)/sizeof(path[0]));

		for (i = 0; i < strlen(path); i++) {
			if (path[i] == '\\') last_slash = i;
		}

		path[last_slash] = 0;

		// service is starting
		serviceStatus.dwCurrentState = SERVICE_START_PENDING;
		SetServiceStatus(serviceStatusHandle, &serviceStatus);

		// do initialisation here
		SetCurrentDirectory(path);

		// running
		serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		serviceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus( serviceStatusHandle, &serviceStatus );

		////////////////////////
		// service main cycle //
		////////////////////////

		argc = 1;

		main(argc, argv);

		// service was stopped
		serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(serviceStatusHandle, &serviceStatus);

		// do cleanup here
		
		// service is now stopped
		serviceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(serviceStatusHandle, &serviceStatus);
	}
}

void Win32_ServiceRun()
{
	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ serviceName, ServiceMain },
		{ 0, 0 }
	};

	if (!StartServiceCtrlDispatcher(serviceTable)) 
	{
	} 
}