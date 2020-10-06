#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <TraceLoggingProvider.h>  
#include <strsafe.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntprov.h>


// Need to link with Wlanapi.lib and Ole32.lib and advapi.lib
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME  _T("ConnectToBestWifiService \n")

//Handle for log file
HANDLE hFile;

void logMessage(char message[512])
{

    DWORD dwBytesToWrite = (DWORD)strlen(message);
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = FALSE;

    if (INVALID_HANDLE_VALUE == hFile) {
        OutputDebugString(_T("Unable to write to log file - file handle is invalid \n"));
        return;
    }

    bErrorFlag = WriteFile(
        hFile,
        message,
        dwBytesToWrite,
        &dwBytesWritten,
        NULL);

    if (FALSE == bErrorFlag)
    {
        OutputDebugString(_T("Unable to write to log file \n"));
    }
    else
    {
        if (dwBytesWritten != dwBytesToWrite)
        {
            // This is an error because a synchronous write that results in
            // success (WriteFile returns TRUE) should write all data as
            // requested. This would not necessarily be the case for
            // asynchronous writes.
            OutputDebugString(_T("Error: dwBytesWritten != dwBytesToWrite\n"));
        }
    }
}

int _tmain(int argc, TCHAR* argv[])
{
    OutputDebugString(_T("ConnectToBestWifiService: Main: Entry \n"));

    //setupTraceLogging();

    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {(LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        OutputDebugString(_T("ConnectToBestWifiService: Main: StartServiceCtrlDispatcher returned error \n"));
        return GetLastError();
    }

    OutputDebugString(_T("ConnectToBestWifiService: Main: Exit \n"));
    return 0;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    DWORD Status = E_FAIL;

    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Entry \n"));

    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: RegisterServiceCtrlHandler returned error \n"));
        return;
    }

    // Tell the service controller we are starting
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: SetServiceStatus returned error \n"));
    }

    /*
     * Perform tasks neccesary to start the service here
     */
    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Performing Service Start Operations \n"));

    // Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error \n"));

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: SetServiceStatus returned error \n"));
        }
        return;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: SetServiceStatus returned error \n"));
    }

    // Start the thread that will perform the main task of the service
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Waiting for Worker Thread to complete \n"));

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject(hThread, INFINITE);

    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Worker Thread Stop Event signaled \n"));


    /*
     * Perform any cleanup tasks
     */
    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Performing Cleanup Operations \n"));

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: SetServiceStatus returned error \n"));
    }

EXIT:
    OutputDebugString(_T("ConnectToBestWifiService: ServiceMain: Exit \n"));

    return;
}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    OutputDebugString(_T("ConnectToBestWifiService: ServiceCtrlHandler: Entry \n"));

    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:

        OutputDebugString(_T("ConnectToBestWifiService: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request \n"));

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        /*
         * Perform tasks neccesary to stop the service here
         */

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("ConnectToBestWifiService: ServiceCtrlHandler: SetServiceStatus returned error \n"));
        }

        // Close Log file handle
        CloseHandle(hFile);

        // This will signal the worker thread to start shutting down
        SetEvent(g_ServiceStopEvent);

        break;

    default:
        break;
    }

    OutputDebugString(_T("ConnectToBestWifiService: ServiceCtrlHandler: Exit"));
}

int ConnectToBestWifi(HANDLE hClient) {

    DWORD dwRetVal = 0;

    int iRet = 0;

    WCHAR GuidString[39] = { 0 };

    unsigned int i, j, k;

    /* variables used for WlanEnumInterfaces  */

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    PWLAN_INTERFACE_INFO pIfInfo = NULL;

    PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;
    PWLAN_AVAILABLE_NETWORK pBssEntry = NULL;

    DWORD dwResult = 0;
    int iRSSI = 0;

    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);

    if (dwResult != ERROR_SUCCESS) {
        char msgbuf[512];
        sprintf_s(msgbuf, "WlanEnumInterfaces failed with error: %u\n", dwResult);
        OutputDebugStringA((LPCSTR)msgbuf);
        logMessage(msgbuf);
        return 1;
    }
    else {

        char msgbuf[512];
        sprintf_s(msgbuf, "\n\n Num Of Interfaces: %lu\n", pIfList->dwNumberOfItems);
        OutputDebugStringA((LPCSTR)msgbuf);
        logMessage(msgbuf);

        unsigned int sleepIntervalInSeconds = 0;

        for (i = 0; i < (int)pIfList->dwNumberOfItems; i++) {

            unsigned int bestSignalQuality = 0;
            WCHAR* bestProfile = NULL;
            WCHAR* currentlyConnectedProfile = NULL;

            pIfInfo = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[i];

            iRet = StringFromGUID2(pIfInfo->InterfaceGuid, (LPOLESTR)&GuidString,
                sizeof(GuidString) / sizeof(*GuidString));
            if (iRet == 0) {
                char msgbuf[512];
                sprintf_s(msgbuf, "StringFromGUID2 failed\n");
                OutputDebugStringA((LPCSTR)msgbuf);
                logMessage(msgbuf);
            }
            else {
                char msgbuf[512];
                sprintf_s(msgbuf, "InterfaceGUID[%d]: %ws\n", i, GuidString);
                OutputDebugStringA((LPCSTR)msgbuf);
                logMessage(msgbuf);
            }

            char msgbuf[512];
            sprintf_s(msgbuf, "Interface Description[%d]: %ws \n", i,
                pIfInfo->strInterfaceDescription);
            OutputDebugStringA((LPCSTR)msgbuf);
            logMessage(msgbuf);

            OutputDebugString(_T("\n"));

            dwResult = WlanScan(hClient,
                &pIfInfo->InterfaceGuid,
                NULL,
                NULL,
                NULL);

            if (dwResult != ERROR_SUCCESS) {

                char msgbuf[512];
                sprintf_s(msgbuf, "WlanScan failed with error : % u\n",
                    dwResult);
                OutputDebugStringA((LPCSTR)msgbuf);
                logMessage(msgbuf);
                return 1;
            }

            
            dwResult = WlanGetAvailableNetworkList(hClient,
                &pIfInfo->InterfaceGuid,
                WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES || WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_MANUAL_HIDDEN_PROFILES,
                NULL,
                &pBssList);

            if (dwResult != ERROR_SUCCESS) {

                char msgbuf[512];
                sprintf_s(msgbuf, "WlanGetAvailableNetworkList failed with error : % u\n",
                    dwResult);
                OutputDebugStringA((LPCSTR)msgbuf);
                logMessage(msgbuf);

                dwRetVal = 1;
            }
            else {
                OutputDebugString(_T("WLAN_AVAILABLE_NETWORK_LIST for this interface\n"));

                char msgbuf[512];
                sprintf_s(msgbuf, "\nNum Of Available Wifi Networks: %lu\n", pBssList->dwNumberOfItems);
                OutputDebugStringA((LPCSTR)msgbuf);
                logMessage(msgbuf);

                for (j = 0; j < pBssList->dwNumberOfItems; j++) {
                    pBssEntry =
                        (WLAN_AVAILABLE_NETWORK*)&pBssList->Network[j];

                    char msgbuf[512];
                    sprintf_s(msgbuf, "Profile Name[%u]: %ws\n", j, pBssEntry->strProfileName);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);

                    sprintf_s(msgbuf, "SSID[%u]: \t", j);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);

                    if (pBssEntry->dot11Ssid.uSSIDLength == 0)
                        OutputDebugString(_T("\n"));
                    else {
                        for (k = 0; k < pBssEntry->dot11Ssid.uSSIDLength; k++) {
                            char msgbuf[512];
                            sprintf_s(msgbuf, "%c", (int)pBssEntry->dot11Ssid.ucSSID[k]);
                            OutputDebugStringA((LPCSTR)msgbuf);
                            logMessage(msgbuf);
                        }
                        OutputDebugString(_T("\n"));
                        logMessage((char *)"\n");
                    }

                    sprintf_s(msgbuf, "  Signal Quality[%u]:\t %u \n", j,
                        pBssEntry->wlanSignalQuality);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);

                    if (pBssEntry->wlanSignalQuality > bestSignalQuality && pBssEntry->strProfileName[0] != '\0' && pBssEntry->bNetworkConnectable) {
                        bestSignalQuality = pBssEntry->wlanSignalQuality;
                        bestProfile = pBssEntry->strProfileName;
                    }

                    if (pBssEntry->dwFlags) {
                        if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
                            OutputDebugString(_T(" - Currently connected"));
                            logMessage((char *)" - Currently connected");
                            currentlyConnectedProfile = pBssEntry->strProfileName;
                        }

                        if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE)
                            OutputDebugString(_T(" - Has profile"));
                            logMessage((char*)" - Has profile");
                    }

                    OutputDebugString(_T("\n\n"));
                    logMessage((char*)"\n\n");
                }
            }

            sprintf_s(msgbuf, "Best signal quality %u, best profile: %ws\n", bestSignalQuality, bestProfile);
            OutputDebugStringA((LPCSTR)msgbuf);
            logMessage(msgbuf);

            if (bestProfile == NULL) {
                continue;
            }

            if (currentlyConnectedProfile != NULL && *currentlyConnectedProfile == *bestProfile) {
                OutputDebugString(_T("Already connected to the best available network \n"));
                logMessage((char*)"Already connected to the best available network \n");
            }

            if (currentlyConnectedProfile == NULL || *currentlyConnectedProfile != *bestProfile) {

                //Disconnect the currently connected network

                dwResult = WlanDisconnect(hClient, &pIfInfo->InterfaceGuid, NULL);
                if (dwResult != ERROR_SUCCESS) {
                    char msgbuf[512];
                    sprintf_s(msgbuf, "WlanDisconnect failed with error: %u\n", dwResult);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);
                    return 1;
                }
                else {
                    OutputDebugString(_T("Successfully disconnected currently connected network \n"));
                    logMessage((char*)"Successfully disconnected currently connected network \n");
                }

                //Connect to network with best signal quality

                WLAN_CONNECTION_PARAMETERS cp;
                memset(&cp, 0, sizeof(WLAN_CONNECTION_PARAMETERS));
                cp.wlanConnectionMode = wlan_connection_mode_profile;
                cp.strProfile = bestProfile;
                cp.dwFlags = 0;
                cp.pDot11Ssid = NULL;
                cp.pDesiredBssidList = 0;
                cp.dot11BssType = dot11_BSS_type_infrastructure;
                dwResult = WlanConnect(hClient, &pIfInfo->InterfaceGuid, &cp, NULL);
                if (dwResult != ERROR_SUCCESS) {
                    char msgbuf[512];
                    sprintf_s(msgbuf, "Wlan connect failed with error: %u\n", dwResult);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);
                    return 1;
                }
                else {
                    char msgbuf[512];
                    sprintf_s(msgbuf, "Successfully connected to %ws \n\n\n", cp.strProfile);
                    OutputDebugStringA((LPCSTR)msgbuf);
                    logMessage(msgbuf);
                }
            }

            if (bestSignalQuality < 50) {
                sleepIntervalInSeconds = 30;
            }
            else {
                sleepIntervalInSeconds = 60;
            }
        }
        Sleep(sleepIntervalInSeconds * 1000);

        if (pBssList != NULL) {
            WlanFreeMemory(pBssList);
            pBssList = NULL;
        }

        if (pIfList != NULL) {
            WlanFreeMemory(pIfList);
            pIfList = NULL;
        }
        return 0;
    }
}



DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    Sleep(60000); // uncomment this to debug the app
    OutputDebugString(_T("ConnectToBestWifiService: ServiceWorkerThread: Entry \n"));

    hFile = CreateFile(L"ConnectToBestWifiService.log",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(_T("ConnectToBestWifiService: ServiceWorkerThread: Unable to open log file for writing \n"));
    }
    
    WCHAR Path[512];
    GetFinalPathNameByHandle(hFile, Path, 512, VOLUME_NAME_NT);
           
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    HANDLE hClient = NULL;
    DWORD dwResult = 0;

    dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) {
        char msgbuf[512];
        sprintf_s(msgbuf, "WlanOpenHandle failed with error: %u\n", dwResult);
        OutputDebugStringA((LPCSTR)msgbuf);
        return 1;
    }

    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
    {
        ConnectToBestWifi(hClient);
    }

    OutputDebugString(_T("ConnectToBestWifiService: ServiceWorkerThread: Exit\n"));

    return ERROR_SUCCESS;
}

