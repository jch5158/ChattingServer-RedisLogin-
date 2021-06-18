#include "stdafx.h"

CChattingServer::CChattingServer(void)
	: mbStopFlag(FALSE)
	, mbUpdateLoopFlag(TRUE)
	, mPlayerCount(0)
	, mAuthenticThreadID(0)
	, mUpdateThreadID(0)
	, mUpdateThreadHandle(INVALID_HANDLE_VALUE)
	, mAuthenticThreadHandle(INVALID_HANDLE_VALUE)
	, mUpdateEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, mAuthenticEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, mSectorAround{ 0, }
	, mJobQueue()
	, mAuthenticJobQueue()
	, mConnectionStateMap()
	, mPlayerMap()
	, mSectorArray()
{
	CRedisConnector::CallWSAStartup();

	// ���͸� �̸� ���س��´�.
	for (INT indexY = 0; indexY < chattingserver::SECTOR_MAX_HEIGHT; ++indexY)
	{
		for (INT indexX = 0; indexX < chattingserver::SECTOR_MAX_WIDTH; ++indexX)
		{
			getSectorAround(indexY, indexX, &mSectorAround[indexY][indexX]);	
		}
	}
}

CChattingServer::~CChattingServer(void)
{
	closeWaitUpdateThread();

	closeWaitAuthenticThread();

	CloseHandle(mUpdateEvent);

	CloseHandle(mAuthenticEvent);

	CRedisConnector::CallWSACleanup();
}

BOOL CChattingServer::setupUpdateThread(void)
{
	mUpdateThreadHandle = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)ExecuteUpdateThread, this, NULL, (UINT*)mUpdateThreadID);
	if (mUpdateThreadHandle == INVALID_HANDLE_VALUE)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"[setupUpdateThread] _beginthreadex Error : %d", GetLastError());

		return FALSE;
	}

	return TRUE;

}

BOOL CChattingServer::setupAuthenticThread(void)
{
	mAuthenticThreadHandle = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)ExecuteAuthenticThread, this, NULL, (UINT*)mAuthenticThreadID);
	if (mAuthenticThreadHandle == INVALID_HANDLE_VALUE)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"[setupAuthenticThread] _beginthreadex Error : %d", GetLastError());

		return FALSE;
	}

	return TRUE;
}


void CChattingServer::closeWaitUpdateThread(void)
{
	if (mUpdateThreadHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	if (WaitForSingleObject(mUpdateThreadHandle, INFINITE) != WAIT_OBJECT_0)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"[closeWaitUpdateThread] WaitForSingleObject Error : %d", GetLastError());

		CCrashDump::Crash();
	}

	CloseHandle(mUpdateThreadHandle);

	mUpdateThreadHandle = INVALID_HANDLE_VALUE;
	
	return;
}

void CChattingServer::closeWaitAuthenticThread(void)
{
	if (mAuthenticThreadHandle = INVALID_HANDLE_VALUE)
	{
		return;
	}

	if (WaitForSingleObject(mAuthenticThreadHandle, INFINITE) != WAIT_OBJECT_0)
	{
		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"[closeWaitAuthenticThread] WaitForSingleObject Error : %d", GetLastError());

		CCrashDump::Crash();
	}

	CloseHandle(mAuthenticThreadHandle);

	mAuthenticThreadHandle = INVALID_HANDLE_VALUE;

	return;
}

DWORD CChattingServer::ExecuteUpdateThread(void* pParam)
{
	CChattingServer* pChattingServer = (CChattingServer*)pParam;

	pChattingServer->UpdateThread();
		
	return 1;
}

void CChattingServer::UpdateThread(void)
{
	CJob* pJob = nullptr;
	
	while (mbUpdateLoopFlag == TRUE)
	{
		if (mJobQueue.Dequeue(&pJob) == FALSE)
		{
			if (WaitForSingleObject(mUpdateEvent, INFINITE) != WAIT_OBJECT_0)
			{
				CCrashDump::Crash();
			}

			continue;
		}

		{
			//CPerformanceProfiler profiler(L"Update Thread");

			jobProcedure(pJob);

			pJob->Free();

			InterlockedIncrement(&mUpdateTPS);
		}
	}

	return;
}

DWORD CChattingServer::ExecuteAuthenticThread(void* pParam)
{
	CChattingServer* pChattingServer = (CChattingServer*)pParam;

	pChattingServer->AuthenticThread();

	return 1;
}


void CChattingServer::AuthenticThread(void)
{	
	mRedisConnector.Connect();

	for (;;)
	{
		INT useSize = mAuthenticJobQueue.GetUseSize();
		if (useSize == 0)
		{
			if(WaitForSingleObject(mAuthenticEvent, INFINITE) != WAIT_OBJECT_0)
			{
				CCrashDump::Crash();
			}

			continue;
		}

		INT directDequeueSize = mAuthenticJobQueue.GetDirectDequeueSize();

		INT remainSize = useSize - directDequeueSize;

		CJob **pAuthenticJobArray = mAuthenticJobQueue.GetFrontBufferPtr();

		CJob* pAuthenticJob;

		for (INT index = 0; index < directDequeueSize; ++index)
		{
			pAuthenticJob = pAuthenticJobArray[index];

			pAuthenticJob->mJobType = CJob::eJobType::LOGIN_JOB;
			
			// ���� ������ ��ū ������ ���� �ʴ´�.
			if (pAuthenticJob->mAccountNumber > 999999)
			{
				if (mRedisConnector.CompareToken(pAuthenticJob->mAccountNumber, pAuthenticJob->mSessionKey, 64) == TRUE)
				{
					pAuthenticJob->mLoginResult = TRUE;
				}
				else
				{
					pAuthenticJob->mLoginResult = FALSE;
				}
			}
			else
			{
				pAuthenticJob->mLoginResult = TRUE;
			}

			mJobQueue.Enqueue(pAuthenticJob);

			SetEvent(mUpdateEvent);
		}

		if (remainSize > 0)
		{
			pAuthenticJobArray = mAuthenticJobQueue.GetBufferPtr();

			for (INT index = 0; index < remainSize; ++index)
			{
				pAuthenticJob = pAuthenticJobArray[index];

				pAuthenticJob->mJobType = CJob::eJobType::LOGIN_JOB;

				if (mRedisConnector.CompareToken(pAuthenticJob->mAccountNumber, pAuthenticJob->mSessionKey, 64) == TRUE)
				{
					pAuthenticJob->mLoginResult = TRUE;
				}
				else
				{
					pAuthenticJob->mLoginResult = FALSE;
				}

				mJobQueue.Enqueue(pAuthenticJob);

				SetEvent(mUpdateEvent);
			}
		}
		else
		{
			remainSize = 0;
		}

		mAuthenticJobQueue.MoveFront(directDequeueSize + remainSize);
	}

	mRedisConnector.Disconnect();

	return;
}


BOOL CChattingServer::insertPlayerMap(UINT64 accountNo, CPlayer* pPlayer)
{
	return mPlayerMap.insert(std::make_pair(accountNo,pPlayer)).second;
}


BOOL CChattingServer::erasePlayerMap(UINT64 accountNo)
{
	return mPlayerMap.erase(accountNo);
}


CPlayer* CChattingServer::findPlayerFromPlayerMap(UINT64 accountNo)
{	
	auto iter = mPlayerMap.find(accountNo);
	if (iter == mPlayerMap.end())
	{
		return nullptr;
	}

	return iter->second;
}

BOOL CChattingServer::insertPlayerSectorMap(INT sectorPosX, INT sectorPosY, UINT64 accountNo, CPlayer* pPlayer)
{
	if (sectorPosX >= chattingserver::SECTOR_MAX_WIDTH || sectorPosY >= chattingserver::SECTOR_MAX_HEIGHT)
	{
		return FALSE;
	}

	return mSectorArray[sectorPosY][sectorPosX].insert(std::make_pair(accountNo, pPlayer)).second;
}


BOOL CChattingServer::erasePlayerSectorMap(INT sectorPosX, INT sectorPosY, UINT64 accountNo)
{
	if (sectorPosX >= chattingserver::SECTOR_MAX_WIDTH || sectorPosY >= chattingserver::SECTOR_MAX_HEIGHT)
	{
		return FALSE;
	}

	return mSectorArray[sectorPosY][sectorPosX].erase(accountNo);
}


DWORD CChattingServer::GetJobQueueStatus(void) const
{
	return mJobQueue.GetUseSize();
}


DWORD CChattingServer::GetPlayerCount(void)
{
	return mPlayerCount;
}

BOOL CChattingServer::OnStart(void)
{
	if (setupUpdateThread() == FALSE)
	{
		return FALSE;
	}

	if (setupAuthenticThread() == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}


// �ű� ������ �����Ͽ����� JobQueue�� Job�� ������ �˷��ش�.
void CChattingServer::OnClientJoin(UINT64 sessionID)
{
	CJob* pJob = createJob(sessionID, CJob::eJobType::CLIENT_JOIN_JOB);

	mJobQueue.Enqueue(pJob);

	SetEvent(mUpdateEvent);

	return;
}


// ������ �����Ͽ����� JobQueue�� ���� �˷��ش�.
void CChattingServer::OnClientLeave(UINT64 sessionID)
{
	CJob* pJob = createJob(sessionID, CJob::eJobType::CLIENT_LEAVE_JOB);

	mJobQueue.Enqueue(pJob);

	SetEvent(mUpdateEvent);

	return;
}


void CChattingServer::OnStartAcceptThread(void)
{

	return;
}

void CChattingServer::OnStartWorkerThread(void)
{

	return;
}

void CChattingServer::OnRecv(UINT64 sessionID, CMessage* pMessage)
{			
	CJob* pJob = createJob(sessionID, CJob::eJobType::MESSAGE_JOB, pMessage);

	pMessage->AddReferenceCount();

	mJobQueue.Enqueue(pJob);

	SetEvent(mUpdateEvent);

	return;
}


// ȭ��Ʈ IP
BOOL CChattingServer::OnConnectionRequest(const WCHAR* userIP, WORD userPort)
{

	return TRUE;
}


void CChattingServer::OnCloseWorkerThread(void)
{

	return;
}

void CChattingServer::OnCloseAcceptThread(void)
{

	return;
}



void CChattingServer::OnError(DWORD errorCode,const WCHAR* errorMessage)
{


}

void CChattingServer::OnStop(void)
{	
	CJob* pJob = createJob(0, CJob::eJobType::SERVER_STOP_JOB, nullptr);

	mJobQueue.Enqueue(pJob);

	SetEvent(mUpdateEvent);

	return;
}

void CChattingServer::SetLoginClient(CLanLoginClient* pLanLoginClient)
{
	mpLanLoginClient = pLanLoginClient;

	return;
}


LONG CChattingServer::GetUpdateTPS(void) const
{
	return mUpdateTPS;
}


void CChattingServer::InitializeUpdateTPS(void)
{
	InterlockedExchange(&mUpdateTPS, 0);

	return;
}

void CChattingServer::jobProcedure(CJob* pJob)
{
	switch (pJob->mJobType)
	{
	case CJob::eJobType::CLIENT_JOIN_JOB:

		jobProcedureClientJoin(pJob->mSessionID);

		return;
	case CJob::eJobType::LOGIN_JOB:
		
		jobProcedureLoginResponse(pJob->mSessionID, pJob);

		return;
	case CJob::eJobType::MESSAGE_JOB:

		jobProcedureMessage(pJob->mSessionID, pJob->mpMessage);

		return;
	case CJob::eJobType::CLIENT_LEAVE_JOB:
		
		jobProcedureClientLeave(pJob->mSessionID);

		return;

	case CJob::eJobType::SERVER_STOP_JOB:

		jobProcedureServerStop();

		return;

	default:

		CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"[jobProcedure] jobType : %d", pJob->mJobType);

		CCrashDump::Crash();

		return;
	}

	return;
}

void CChattingServer::jobProcedureMessage(UINT64 sessionID, CMessage* pMessage)
{
	WORD messageType;

	*pMessage >> messageType;

	if (recvProcedure(sessionID, messageType, pMessage) == FALSE)
	{
		Disconnect(sessionID);
	}

	pMessage->Free();
	
	return;
}


CJob* CChattingServer::createJob(UINT64 sessionID, CJob::eJobType jobType, CMessage* pMessage)
{
	CJob* pJob = CJob::Alloc();

	pJob->mSessionID = sessionID;

	pJob->mJobType = jobType;

	pJob->mpMessage = pMessage;

	return pJob;
}


//
//
//CPlayer* CChattingServer::findPlayer(UINT64 sessionID)
//{
//	auto iter = mPlayerMap.find(sessionID);
//
//	if (iter == mPlayerMap.end())
//	{
//		return nullptr;
//	}
//
//	return iter->second;
//}

CChattingServer::CConnectionState* CChattingServer::findConnectionState(UINT64 sessionID)
{
	auto iter = mConnectionStateMap.find(sessionID);

	if (iter == mConnectionStateMap.end())
	{
		return nullptr;
	}
	
	return iter->second;
}


void CChattingServer::createPlayer(UINT64 accountNumber, UINT64 sessionID, CPlayer** pPlayer)
{
	CPlayer *pDuplicatePlayer = findPlayerFromPlayerMap(accountNumber);
	if (pDuplicatePlayer != nullptr)
	{
		// ���� ����
		Disconnect(pDuplicatePlayer->mSessionID);

		if (pDuplicatePlayer->mbSectorSetFlag == TRUE)
		{
			// ���Ϳ��� ����
			erasePlayerSectorMap(pDuplicatePlayer->mSectorX, pDuplicatePlayer->mSectorY, accountNumber);
		}

		// �÷��̾� �ʿ��� ����
		erasePlayerMap(accountNumber);
	}

	*pPlayer = CPlayer::Alloc();

	(*pPlayer)->mAccountNumber = accountNumber;

	(*pPlayer)->mSessionID = sessionID;

	return;
}

void CChattingServer::deletePlayer(UINT64 accountNumber, UINT64 sessionID)
{
	CPlayer* pPlayer = findPlayerFromPlayerMap(accountNumber);
	if (pPlayer == nullptr)
	{	
		return;
	}

	do
	{
		if (pPlayer->mSessionID != sessionID)
		{
			break;
		}

		removeSector(pPlayer);

		erasePlayerMap(accountNumber);

	} while (0);
	
	--mPlayerCount;

	pPlayer->Free();

	return;
}


void CChattingServer::removeSector(CPlayer* pPlayer)
{
	// ���� �ڸ� ���� ���̶���� return
	if (pPlayer->mbSectorSetFlag == FALSE)
	{
		// �ڸ����� Flag�� TRUE�� �������ش�.
		pPlayer->mbSectorSetFlag = TRUE;

		return;
	}

	erasePlayerSectorMap(pPlayer->mSectorX, pPlayer->mSectorY, pPlayer->mAccountNumber);

	return;
}


// Sector ���� ����� ���ؼ� ���ڴ� short�� �޴´�.
void CChattingServer::getSectorAround(INT posY, INT posX, stSectorAround* pSectorAround)
{	
	posY -= 1;
	posX -= 1;

	for (INT countY = 0; countY < 3; ++countY)
	{
		if (posY + countY < 0 || posY + countY >= chattingserver::SECTOR_MAX_HEIGHT)
		{
			continue;
		}

		for (INT countX = 0; countX < 3; ++countX)
		{
			if (posX + countX < 0 || posX + countX >= chattingserver::SECTOR_MAX_WIDTH)
			{
				continue;
			}

			pSectorAround->aroundSector[pSectorAround->count].posY = posY + countY;
			pSectorAround->aroundSector[pSectorAround->count].posX = posX + countX;

			++pSectorAround->count;
		}
	}
}


void CChattingServer::sendOneSector(stSector* pSector, CMessage* pMessage)
{
	std::unordered_map<UINT64, CPlayer*>* pPlayerMap = &mSectorArray[pSector->posY][pSector->posX];

	// ������� for���� ����Ѵ�.
	for (auto& iter : *pPlayerMap)
	{
		//++gSendPacketAround;
		SendPacket(iter.second->mSessionID, pMessage);
	}

	return;
}

void CChattingServer::sendAroundSector(stSectorAround* pSectorAround, CMessage* pMessage)
{
	//CPerformanceProfiler profiler(L"sendAroundSector");

	stSector* pSectorArray = pSectorAround->aroundSector;

	INT count = pSectorAround->count;

	for (INT index = 0; index < count; ++index)
	{		
		sendOneSector(&pSectorArray[index], pMessage);
	}
}

BOOL CChattingServer::recvProcedureLoginRequest(UINT64 sessionID, CMessage* pMessage)
{		
	CConnectionState* pConnectionState = findConnectionState(sessionID);
	
	// �α��� ó���� �����Ͽ��� ������ TRUE �� �����Ѵ�.
	pConnectionState->mbLoginProcFlag = TRUE;

	CJob* pAuthenticJob =CJob::Alloc();

	pAuthenticJob->mSessionID = sessionID;

	*pMessage >> pConnectionState->mAccountNo;

	pAuthenticJob->mAccountNumber = pConnectionState->mAccountNo;

	pMessage->GetPayload((CHAR*)pAuthenticJob->mPlayerID, player::PLAYER_STRING_LENGTH * sizeof(WCHAR));

	pMessage->MoveReadPos(player::PLAYER_STRING_LENGTH * sizeof(WCHAR));

	pMessage->GetPayload((CHAR*)pAuthenticJob->mPlayerNickName, player::PLAYER_STRING_LENGTH * sizeof(WCHAR));

	pMessage->MoveReadPos(player::PLAYER_STRING_LENGTH * sizeof(WCHAR));

	pMessage->GetPayload((CHAR*)pAuthenticJob->mSessionKey, 64);

	pMessage->MoveReadPos(64);

	if (mAuthenticJobQueue.Enqueue(&pAuthenticJob) == FALSE)
	{
		pMessage->Free();

		return FALSE;
	}

	SetEvent(mAuthenticEvent);

	return TRUE;
}


void CChattingServer::jobProcedureLoginResponse(UINT64 sessionID, CJob* pJob)
{
	CConnectionState* pConnectionState = findConnectionState(sessionID);

	// OnClientLeave�� ȣ��Ǿ����� erase�� CConnectionState�� �������ش�.
	if (pConnectionState->mbConnectionFlag == FALSE)
	{
		mConnectionStateMap.erase(sessionID);
	
		pConnectionState->Free();

		return;
	}

	// �α��� ó���� �����ٸ� FALSE�� �����Ѵ�.
	pConnectionState->mbLoginProcFlag = FALSE;

	CPlayer* pPlayer;

	createPlayer(pJob->mAccountNumber, sessionID, &pPlayer);

	wcscpy_s(pPlayer->mPlayerID, pJob->mPlayerID);

	wcscpy_s(pPlayer->mPlayerNickName, pJob->mPlayerNickName);

	if (insertPlayerMap(pPlayer->mAccountNumber, pPlayer) == FALSE)
	{
		CCrashDump::Crash();
	}

	++mPlayerCount;

	sendLoginResponse(pJob->mLoginResult, pPlayer);

	mpLanLoginClient->NotificationClientLoginSuccess(pJob->mAccountNumber);

	return;
}


void CChattingServer::sendLoginResponse(bool bLoginFlag, CPlayer *pPlayer)
{
	CMessage* pMessage = CMessage::Alloc();

	// �α��� �Ϸ� �޽����� �����.
	packingLoginMessage(bLoginFlag, pPlayer->mAccountNumber, pMessage);

	// �α��� �Ϸ� �޽����� �����Ѵ�.
	SendPacket(pPlayer->mSessionID, pMessage);

	pMessage->Free();

	return;
}




void CChattingServer::packingLoginMessage(bool bLoginFlag, UINT64 accountNumber, CMessage* pMessage)
{
	*pMessage << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << bLoginFlag << accountNumber;

	return;
}


// ���� �̵� 
BOOL CChattingServer::recvProcedureSectorMoveRequest(UINT64 sessionID, CMessage* pMessage)
{
	UINT64 accountNumber;
	WORD sectorX;
	WORD sectorY;

	*pMessage >> accountNumber >> sectorX >> sectorY;

	
	// �α������� �ʾҴµ� ���� �̵���û�� �������� ���´�.
	CPlayer* pPlayer = findPlayerFromPlayerMap(accountNumber);
	if (pPlayer == nullptr)
	{	
		return FALSE;
	}

	// ��ī��Ʈ �ѹ��� ��ġ�ϴ��� Ȯ���Ѵ�. ��ġ���� �ʴٸ��� return FALSE
	//if (pPlayer->mAccountNumber != accountNumber)
	//{
	//	return FALSE;
	//}

	// unsigned �̱� ������ SECTOR_MAX_WIDTH, SECTOR_MAX_HEIGHT ���� ū���� Ȯ���ϸ� �ȴ�.  
	//if (sectorX > SECTOR_MAX_WIDTH || sectorY > SECTOR_MAX_HEIGHT)
	//{
	//	return FALSE;
	//}

	removeSector(pPlayer);

	pPlayer->mSectorX = sectorX;
	pPlayer->mSectorY = sectorY;

	insertPlayerSectorMap(pPlayer->mSectorX, pPlayer->mSectorY, pPlayer->mAccountNumber, pPlayer);

	sendSectorMoveResponse(pPlayer);	

	return TRUE;
}

// ���� �̵� ��� �۽�
void CChattingServer::sendSectorMoveResponse(CPlayer* pPlayer)
{
	CMessage* pMessage = CMessage::Alloc();

	packingSectorMoveMessage(pPlayer->mAccountNumber, pPlayer->mSectorX, pPlayer->mSectorY, pMessage);
	
	SendPacket(pPlayer->mSessionID, pMessage);	

	pMessage->Free();

	return;
}

void CChattingServer::packingSectorMoveMessage(UINT64 accountNumber, WORD sectorX, WORD sectorY, CMessage* pMessage)
{
	*pMessage << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << accountNumber << sectorX << sectorY;

	return;
}


// ä�� �޽��� ��û
BOOL CChattingServer::recvProcedureChatRequest(UINT64 sessionID, CMessage* pMessage)
{

	UINT64 accountNumber;

	WORD chatLength;

	*pMessage >> accountNumber >> chatLength;

	// ���� �α������� �ʾҴٸ��� return FALSE;
	CPlayer* pPlayer = findPlayerFromPlayerMap(accountNumber);
	if (pPlayer == nullptr)
	{
		return FALSE;
	}

	// ���� Ȯ��
	// ��ī��Ʈ �ѹ��� ��ġ���� �������� return FALSE;
	if (pPlayer->mAccountNumber != accountNumber)
	{
		return FALSE;
	}

	// ���� ���� ������ ���� �ʾҴٸ��� return FALSE
	if (pPlayer->mbSectorSetFlag == FALSE)
	{
		return FALSE;
	}

	WCHAR pChat[chattingserver::MAX_CHAT_LENGTH];

	pMessage->GetPayload((CHAR*)pChat, chatLength);

	pMessage->MoveReadPos(chatLength);

	sendChatResponse(chatLength, pChat, pPlayer);

	return TRUE;
}

void CChattingServer::sendChatResponse(WORD chatLength, WCHAR* pChat, CPlayer* pPlayer)
{
	CMessage* pMessage = CMessage::Alloc();

	packingChatMessage(pPlayer->mAccountNumber, pPlayer->mPlayerID, sizeof(WCHAR) * player::PLAYER_STRING_LENGTH, pPlayer->mPlayerNickName, sizeof(WCHAR) * player::PLAYER_STRING_LENGTH, chatLength, pChat, pMessage);

	// ä�� �۽��� �ֺ� ���ͷ� �޽����� �Ѹ���.
	sendAroundSector(&mSectorAround[pPlayer->mSectorY][pPlayer->mSectorX], pMessage);

	pMessage->Free();
	
	return;
}


void CChattingServer::packingChatMessage(UINT64 accountNumber, WCHAR* pPlayerID, DWORD cbPlayerID, WCHAR* pPlayerNickName, DWORD cbPlayerNickName, WORD chatLength, WCHAR* pChat, CMessage* pMessage)
{
	*pMessage << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << accountNumber;
	
	pMessage->PutPayload((CHAR*)pPlayerID, cbPlayerID);

	pMessage->MoveWritePos(cbPlayerID);

	pMessage->PutPayload((CHAR*)pPlayerNickName, cbPlayerNickName);

	pMessage->MoveWritePos(cbPlayerNickName);

	*pMessage << chatLength;

	pMessage->PutPayload((CHAR*)pChat, chatLength);

	pMessage->MoveWritePos(chatLength);

	return;
}

BOOL CChattingServer::recvProcedureHeartbeatRequest(UINT64 sessionID)
{
	CConnectionState* pConnectionState = findConnectionState(sessionID);
	if (pConnectionState == nullptr)
	{
		return FALSE;
	}

	//pConnectionState->mTimeout = timeGetTime();

	return TRUE;
}

void CChattingServer::jobProcedureClientJoin(UINT64 sessionID)
{
	CConnectionState* pConnectionState = CConnectionState::Alloc();

	mConnectionStateMap.insert(std::make_pair(sessionID, pConnectionState));

	return;
}




void CChattingServer::jobProcedureClientLeave(UINT64 sessionID)
{
	CConnectionState* pConnectionState = findConnectionState(sessionID);

	// �α��� ó�����̱� ������ �÷��̾� �������� return �Ѵ�.
	if (pConnectionState->mbLoginProcFlag == TRUE)
	{
		// �α��� response ���� mbConnectionFlag �� FALSE �� ���� Ȯ�� �� �����Ѵ�.
		pConnectionState->mbConnectionFlag = FALSE;

		return;
	}
	else
	{
		mConnectionStateMap.erase(sessionID);

		pConnectionState->Free();
	}

	deletePlayer(pConnectionState->mAccountNo, sessionID);

	if (mbStopFlag == TRUE)
	{
		if (mPlayerMap.empty() == TRUE)
		{
			mbUpdateLoopFlag = FALSE;
		}
	}

	return;
}

void CChattingServer::jobProcedureServerStop(void)
{
	mbStopFlag = TRUE;

	if (mPlayerMap.empty() == TRUE)
	{
		mbUpdateLoopFlag = FALSE;
	}

	return;
}

// �޽��� �������� 
BOOL CChattingServer::recvProcedure(UINT64 sessionID, DWORD messageType, CMessage* pMessage)
{
	switch (messageType)
	{
	// �α��� ��û �޽���
	case en_PACKET_CS_CHAT_REQ_LOGIN:

		return recvProcedureLoginRequest(sessionID, pMessage);

	// ���� �̵� �޽���
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:

		return recvProcedureSectorMoveRequest(sessionID, pMessage);
		
	// ä�� �޽���
	case en_PACKET_CS_CHAT_REQ_MESSAGE:

		return recvProcedureChatRequest(sessionID, pMessage);

	// ��Ʈ��Ʈ �޽��� ( ���� ��� X )
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:

		return recvProcedureHeartbeatRequest(sessionID);

	default:
		
		// ���� Ȯ��		
		return FALSE;
	}

	return TRUE;
}

