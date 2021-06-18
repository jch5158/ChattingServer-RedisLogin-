#pragma once

#include "stdafx.h"

extern CHardwareProfiler hardware;

class CServerController
{

public:

	CServerController(void)
		: mbControlModeFlag(FALSE)
		, mbShutdownFlag(FALSE)
		, mpChatServer(nullptr)
		, mpLoginClient(nullptr)
		, mpMonitoringClient(nullptr)
	{

	}

	~CServerController(void)
	{

	}


	BOOL GetShutdownFlag(void) const
	{
		return mbShutdownFlag;
	}

	void SetChatServer(CChattingServer* pChatServer)
	{
		mpChatServer = pChatServer;

		return;
	}

	void SetMonitoringClient(CLanMonitoringClient* pMonitoringClient)
	{
		mpMonitoringClient = pMonitoringClient;

		return;
	}

	void SetLoginClient(CLanLoginClient* pLoginClient)
	{
		mpLoginClient = pLoginClient;

		return;
	}

	void ServerControling(void)
	{		

		if (_kbhit() == TRUE)
		{
			WCHAR controlKey = _getwch();

			if (controlKey == L'u' || controlKey == L'U')
			{				
				mbControlModeFlag = TRUE;
			}

			if ((controlKey == L'd' || controlKey == L'D') && mbControlModeFlag)
			{
				CCrashDump::Crash();
			}

			if ((controlKey == L'q' || controlKey == L'Q') && mbControlModeFlag)
			{
				mbShutdownFlag = TRUE;
			}

			if (controlKey == L'l' || controlKey == L'L')
			{
				mbControlModeFlag = FALSE;
			}
		}

		wprintf_s(L"\n\n\n\n"
			L"	                                                       [ Chat Server]\n"
			L" 旨收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收旬\n\n"
			L"    ChatServer Bind IP : %s | ChatServer Bind Port : %d | ChatServer Accept Total : %lld | Player Count : %4d \n\n"
			L"    Chat Current Client : %4d / %4d | Running Thread : %d | Worker Thread : %d | Nagle : %d \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Monitoring Bind IP : %s | Monitoring Bind Port : %d | Connect State : %d \n\n"
			L"    Running Thread : %d | Worker Thread : %d | Nagle : %d \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Login Bind IP : %s | Login Bind Port : %d | Connect State : %d \n\n"
			L"    Login Running Thread : %d | Login Worker Thread : %d | Login Nagle : %d \n\n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Control Mode : %d | [ L ] : Control Lock | [ U ] : Control Unlock | [ D ] : Crash | [ Q ] : Exit | LogLevel : %d \n"
			L"  收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收 \n\n"
			L"    Chunk Message Alloc Count : %d | Player Chunk Alloc Count : %d | CConnectionState Chunk Alloc Count : %d\n\n"
			L" 曲收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收收旭\n\n"
			, mpChatServer->GetServerBindIP(), mpChatServer->GetServerBindPort(), mpChatServer->GetAcceptTotal(), mpChatServer->GetPlayerCount()
			, mpChatServer->GetCurrentClientCount(), mpChatServer->GetMaxClientCount(), mpChatServer->GetRunningThreadCount(), mpChatServer->GetWorkerThreadCount(),  mpChatServer->GetNagleFlag()
			, mpMonitoringClient->GetConnectIP(), mpMonitoringClient->GetConnectPort(), mpMonitoringClient->GetConnectStateFlag()
			, mpMonitoringClient->GetRunningThreadCount(), mpMonitoringClient->GetWorkerThreadCount(), mpMonitoringClient->GetNagleFlag()
			, mpLoginClient->GetConnectIP(), mpLoginClient->GetConnectPort(), mpLoginClient->GetConnectStateFlag()
			, mpLoginClient->GetRunningThreadCount(), mpLoginClient->GetWorkerThreadCount(), mpLoginClient->GetNagleFlag()
			, mbControlModeFlag,CSystemLog::GetInstance()->GetLogLevel()
			, CLockFreeObjectFreeList<CTLSLockFreeObjectFreeList<CMessage>::CChunk>::GetAllocNodeCount()
			, CLockFreeObjectFreeList<CTLSLockFreeObjectFreeList<CPlayer>::CChunk>::GetAllocNodeCount()
			, CLockFreeObjectFreeList<CTLSLockFreeObjectFreeList<CChattingServer::CConnectionState>::CChunk>::GetAllocNodeCount()
		);

	}

private:

	BOOL mbControlModeFlag;

	BOOL mbShutdownFlag;

	CLanLoginClient* mpLoginClient;

	CChattingServer* mpChatServer;

	CLanMonitoringClient* mpMonitoringClient;

};

