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

	// 섹터를 미리 구해놓는다.
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
			
			// 더미 계정은 토큰 인증을 하지 않는다.
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


// 신규 세션이 접속하였음을 JobQueue에 Job을 던져서 알려준다.
void CChattingServer::OnClientJoin(UINT64 sessionID)
{
	CJob* pJob = createJob(sessionID, CJob::eJobType::CLIENT_JOIN_JOB);

	mJobQueue.Enqueue(pJob);

	SetEvent(mUpdateEvent);

	return;
}


// 세션이 종료하였음을 JobQueue를 통해 알려준다.
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


// 화이트 IP
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
		// 세션 끊기
		Disconnect(pDuplicatePlayer->mSessionID);

		if (pDuplicatePlayer->mbSectorSetFlag == TRUE)
		{
			// 섹터에서 제거
			erasePlayerSectorMap(pDuplicatePlayer->mSectorX, pDuplicatePlayer->mSectorY, accountNumber);
		}

		// 플레이어 맵에서 제거
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
	// 아직 자리 셋팅 전이라면은 return
	if (pPlayer->mbSectorSetFlag == FALSE)
	{
		// 자리셋팅 Flag를 TRUE로 변경해준다.
		pPlayer->mbSectorSetFlag = TRUE;

		return;
	}

	erasePlayerSectorMap(pPlayer->mSectorX, pPlayer->mSectorY, pPlayer->mAccountNumber);

	return;
}


// Sector 음수 계산을 위해서 인자는 short를 받는다.
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

	// 범위기반 for문을 사용한다.
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
	
	// 로그인 처리에 돌입하였기 때문에 TRUE 로 변경한다.
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

	// OnClientLeave가 호출되었으니 erase로 CConnectionState를 정리해준다.
	if (pConnectionState->mbConnectionFlag == FALSE)
	{
		mConnectionStateMap.erase(sessionID);
	
		pConnectionState->Free();

		return;
	}

	// 로그인 처리가 끝났다면 FALSE로 변경한다.
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

	// 로그인 완료 메시지를 만든다.
	packingLoginMessage(bLoginFlag, pPlayer->mAccountNumber, pMessage);

	// 로그인 완료 메시지를 응답한다.
	SendPacket(pPlayer->mSessionID, pMessage);

	pMessage->Free();

	return;
}




void CChattingServer::packingLoginMessage(bool bLoginFlag, UINT64 accountNumber, CMessage* pMessage)
{
	*pMessage << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << bLoginFlag << accountNumber;

	return;
}


// 섹터 이동 
BOOL CChattingServer::recvProcedureSectorMoveRequest(UINT64 sessionID, CMessage* pMessage)
{
	UINT64 accountNumber;
	WORD sectorX;
	WORD sectorY;

	*pMessage >> accountNumber >> sectorX >> sectorY;

	
	// 로그인하지 않았는데 섹터 이동요청을 보내면은 끊는다.
	CPlayer* pPlayer = findPlayerFromPlayerMap(accountNumber);
	if (pPlayer == nullptr)
	{	
		return FALSE;
	}

	// 어카운트 넘버가 일치하는지 확인한다. 일치하지 않다면은 return FALSE
	//if (pPlayer->mAccountNumber != accountNumber)
	//{
	//	return FALSE;
	//}

	// unsigned 이기 때문에 SECTOR_MAX_WIDTH, SECTOR_MAX_HEIGHT 보다 큰지만 확인하면 된다.  
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

// 섹터 이동 결과 송신
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


// 채팅 메시지 요청
BOOL CChattingServer::recvProcedureChatRequest(UINT64 sessionID, CMessage* pMessage)
{

	UINT64 accountNumber;

	WORD chatLength;

	*pMessage >> accountNumber >> chatLength;

	// 아직 로그인하지 않았다면은 return FALSE;
	CPlayer* pPlayer = findPlayerFromPlayerMap(accountNumber);
	if (pPlayer == nullptr)
	{
		return FALSE;
	}

	// 공격 확인
	// 어카운트 넘버가 일치하지 않으면은 return FALSE;
	if (pPlayer->mAccountNumber != accountNumber)
	{
		return FALSE;
	}

	// 아직 섹터 셋팅을 하지 않았다면은 return FALSE
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

	// 채팅 송신자 주변 섹터로 메시지를 뿌린다.
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

	// 로그인 처리중이기 때문에 플레이어 삭제없이 return 한다.
	if (pConnectionState->mbLoginProcFlag == TRUE)
	{
		// 로그인 response 에서 mbConnectionFlag 가 FALSE 인 것을 확인 후 정리한다.
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

// 메시지 프로토콜 
BOOL CChattingServer::recvProcedure(UINT64 sessionID, DWORD messageType, CMessage* pMessage)
{
	switch (messageType)
	{
	// 로그인 요청 메시지
	case en_PACKET_CS_CHAT_REQ_LOGIN:

		return recvProcedureLoginRequest(sessionID, pMessage);

	// 섹터 이동 메시지
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:

		return recvProcedureSectorMoveRequest(sessionID, pMessage);
		
	// 채팅 메시지
	case en_PACKET_CS_CHAT_REQ_MESSAGE:

		return recvProcedureChatRequest(sessionID, pMessage);

	// 하트비트 메시지 ( 현재 사용 X )
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:

		return recvProcedureHeartbeatRequest(sessionID);

	default:
		
		// 공격 확인		
		return FALSE;
	}

	return TRUE;
}

