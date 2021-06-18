#pragma once


namespace chattingserver
{
	constexpr INT SECTOR_MAX_WIDTH = 50;
	constexpr INT SECTOR_MAX_HEIGHT = 50;
	constexpr INT MAX_CHAT_LENGTH = 200;
}

class CChattingServer : public CNetServer
{
public:

	CChattingServer();

	~CChattingServer();

	DWORD GetJobQueueStatus(void) const;

	DWORD GetPlayerCount(void);

	virtual BOOL OnStart(void) final;

	virtual void OnClientJoin(UINT64 sessionID) final;

	virtual void OnClientLeave(UINT64 sessionID) final;

	virtual void OnStartAcceptThread(void) final;

	virtual void OnStartWorkerThread(void) final;

	virtual void OnRecv(UINT64 sessionID, CMessage* pMessage) final;
	
	virtual BOOL OnConnectionRequest(const WCHAR* userIP, WORD userPort) final;

	virtual void OnCloseWorkerThread(void) final;

	virtual void OnCloseAcceptThread(void) final;

	virtual void OnError(DWORD errorCode, const WCHAR* errorMessage) final;

	virtual void OnStop(void) final;

	void SetLoginClient(CLanLoginClient* pLanLoginClient);

	class CConnectionState
	{
	private:

		template <class DATA>
		friend class CLockFreeObjectFreeList;

		template <class DATA>
		friend class CTLSLockFreeObjectFreeList;


		CConnectionState(void)
			: mbConnectionFlag(TRUE)
			, mbLoginProcFlag(FALSE)
			, mAccountNo(0)
		{
		}

		~CConnectionState(void)
		{
		}

	public:

		static CConnectionState* Alloc(void)
		{
			CConnectionState* pConnectionState = mConnectionFreeList.Alloc();

			pConnectionState->Clear();

			return pConnectionState;
		}

		void Free(void)
		{
			if (mConnectionFreeList.Free(this) == FALSE)
			{
				CSystemLog::GetInstance()->Log(TRUE, CSystemLog::eLogLevel::LogLevelError, L"ChattingServer", L"Free CConnectionState is Failed");

				CCrashDump::Crash();
			}

			return;
		}

		void Clear(void)
		{
			mbConnectionFlag = TRUE;
			mbLoginProcFlag = FALSE;
			mAccountNo = 0;
		}

		// ����Ǿ��ִ��� Ȯ���ϴ� �÷���
		// ���� �����忡�� ó�� �� �÷��̾� ���� �� �� �� �� Ȯ���Ͽ� �÷��̾� ������ �����ִ� �÷���
		BOOL mbConnectionFlag;
		
		// �α��� ó�������� ���θ� Ȯ���ϴ� �÷��� ���� �����忡�� ó�������� ���� Player�� �������� �ʾ� ������ �� ����
		BOOL mbLoginProcFlag;

		UINT64 mAccountNo;

		inline static CTLSLockFreeObjectFreeList<CConnectionState> mConnectionFreeList = { 0,FALSE };
	};

	struct stSector
	{
		LONG posY;
		LONG posX;
	};

	struct stSectorAround
	{
		DWORD count;
		stSector aroundSector[9];
	};

	LONG GetUpdateTPS(void) const;

	void InitializeUpdateTPS(void);

private:

	static DWORD WINAPI ExecuteUpdateThread(void* pParam);

	static DWORD WINAPI ExecuteAuthenticThread(void* pParam);

	void UpdateThread(void);

	void AuthenticThread(void);

	BOOL setupUpdateThread(void);

	BOOL setupAuthenticThread(void);

	void closeWaitUpdateThread(void);

	void closeWaitAuthenticThread(void);


	BOOL insertPlayerMap(UINT64 accountNo, CPlayer* pPlayer);
	BOOL erasePlayerMap(UINT64 accountNo);
	CPlayer* findPlayerFromPlayerMap(UINT64 accountNo);


	BOOL insertPlayerSectorMap(INT sectorPosX, INT sectorPosY, UINT64 accountNo, CPlayer* pPlayer);
	BOOL erasePlayerSectorMap(INT sectorPosX, INT sectorPosY, UINT64 accountNo);
	


	CChattingServer::CConnectionState* findConnectionState(UINT64 sessionID);

	void createPlayer(UINT64 acountNumber, UINT64 sessionID, CPlayer **pPlayer);

	void deletePlayer(UINT64 acountNumber, UINT64 sessionID);

	CJob* createJob(UINT64 sessionID, CJob::eJobType jobType, CMessage* pMessage = nullptr);

	void removeSector(CPlayer* pPlayer);

	void getSectorAround(INT posY, INT posX, stSectorAround* pSectorAround);

	void sendOneSector(stSector* pSector, CMessage* pMessage);

	void sendAroundSector(stSectorAround* pSectorArround, CMessage* pMessage);


	// Job �б⹮ ���� �Լ�
	void jobProcedure(CJob* pJob);

	void jobProcedureClientJoin(UINT64 sessionID);

	void jobProcedureLoginResponse(UINT64 sessionID, CJob* pJob);

	void jobProcedureMessage(UINT64 sessionID, CMessage* pMessage);

	void jobProcedureClientLeave(UINT64 sessionID);

	void jobProcedureServerStop(void);


	// recv �޽��� �б⹮ ���� �Լ�
	BOOL recvProcedure(UINT64 sessionID, DWORD messageType , CMessage* pMessage);

	BOOL recvProcedureLoginRequest(UINT64 sessionID, CMessage* pMessage);

	BOOL recvProcedureSectorMoveRequest(UINT64 sessionID, CMessage* pMessage);

	BOOL recvProcedureChatRequest(UINT64 sessionID, CMessage* pMessage);

	BOOL recvProcedureHeartbeatRequest(UINT64 sessionID);


	// �޽��� ���� �Լ�
	void sendLoginResponse(bool bLoginFlag, CPlayer* pPlayer);

	void sendSectorMoveResponse(CPlayer* pPlayer);

	void sendChatResponse(WORD chatLength, WCHAR* pChat, CPlayer *pPlayer);


	// ���� �޽��� ���� �Լ�
	void packingLoginMessage(bool bLoginFlag, UINT64 accountNumber, CMessage* pMessage);

	void packingSectorMoveMessage(UINT64 accountNumber, WORD sectorX, WORD sectorY, CMessage* pMessage);

	void packingChatMessage(UINT64 accountNumber, WCHAR* pPlayerID, DWORD cbPlayerID, WCHAR* pPlayerNickName, DWORD cbPlayerNickName, WORD chatLength, WCHAR* pChat, CMessage* pMessage);




	LONG mUpdateTPS;

	BOOL mbStopFlag;

	BOOL mbUpdateLoopFlag;

	DWORD mPlayerCount;

	DWORD mAuthenticThreadID;

	DWORD mUpdateThreadID;

	CLanLoginClient* mpLanLoginClient;

	HANDLE mUpdateThreadHandle;

	HANDLE mAuthenticThreadHandle;

	HANDLE mUpdateEvent;

	HANDLE mAuthenticEvent;

	CRedisConnector mRedisConnector;

	// ������ ������ �� �ֺ� ���͸� �̸� ���س��´�.
	stSectorAround mSectorAround[chattingserver::SECTOR_MAX_HEIGHT][chattingserver::SECTOR_MAX_WIDTH];

	CLockFreeQueue<CJob*> mJobQueue;

	CTemplateRingBuffer<CJob*> mAuthenticJobQueue;

	// �α���
	// key : sessionID, value : CConnectionState*
	std::unordered_map<UINT64, CChattingServer::CConnectionState*> mConnectionStateMap;

	// key : accountNo, value : CPlayer*
	std::unordered_map<UINT64, CPlayer*> mPlayerMap;

	// key : accountNo, value : CPlayer*
	std::unordered_map<UINT64, CPlayer*> mSectorArray[chattingserver::SECTOR_MAX_HEIGHT][chattingserver::SECTOR_MAX_WIDTH];
	
};

