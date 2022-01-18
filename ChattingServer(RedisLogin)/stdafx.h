#pragma once


#include "RedisLibrary/RedisLibrary/CTLSRedisConnector.h"
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <iostream>
#include <Windows.h>
#include <process.h>
#include <time.h>
#include <conio.h>
#include <locale.h>
#include <string>

#include "DumpLibrary/DumpLibrary/CCrashDump.h"
#include "SystemLogLibrary/SystemLogLibrary/CSystemLog.h"
#include "PerformanceProfiler/PerformanceProfiler/CPerformanceProfiler.h"
#include "PerformanceProfiler/PerformanceProfiler/CTLSPerformanceProfiler.h"
#include "TPSProfiler/TPSProfiler/CTPSProfiler.h"
#include "ParserLibrary/ParserLibrary/CParser.h"
#include "CPUProfiler/CPUProfiler/CCPUProfiler.h"
#include "HardwareProfilerLibrary/HardwareProfilerLibrary/CHardwareProfiler.h"

#include "MessageLibrary/MessageLibrary/CMessage.h"
#include "RingBufferLibrary/RingBufferLib/CRingBuffer.h"
#include "RingBufferLibrary/RingBufferLib/CTemplateQueue.h"
#include "LockFreeObjectFreeList/ObjectFreeListLib/CLockFreeObjectFreeList.h"
#include "LockFreeObjectFreeList/ObjectFreeListLib/CTLSLockFreeObjectFreeList.h"
#include "LockFreeStack/LockFreeStackLib/CLockFreeStack.h"
#include "LockFreeQueue/LockFreeQueueLib/CLockFreeQueue.h"

#include "CommonProtocol.h"
#include "NetworkEngine/NetServerEngine/NetServer/CNetServer.h"
#include "NetworkEngine/LanClientEngine/LanClientEngine/CLanClient.h"

#include "CPlayer.h"
#include "CJob.h"
#include "CLanLoginClient.h"

#include "CChattingServer.h"
#include "CLanMonitoringClient.h"

#pragma comment (lib,"Ws2_32.lib")
#pragma comment (lib,"Winmm.lib")
#pragma comment(lib,"Mswsock.lib")


