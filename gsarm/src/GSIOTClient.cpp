#include "GSIOTClient.h"
#include "common.h"
#include "RunCode.h"
#include "XmppRegister.h"
#include "disco.h"
#include "message.h"
#include "rostermanager.h"
#include "GSIOTInfo.h"
#include "GSIOTDevice.h"
#include "GSIOTControl.h"
#include "GSIOTDeviceInfo.h"
#include "GSIOTHeartbeat.h"
#include "MediaControl.h"
#include "XmppGSResult.h"
#include "AutoEventthing.h"
#include "XmppGSMessage.h"
#include "APlayer.h"
#include "HeartbeatMon.h"
#include "XmppGSChange.h"

#include <Psapi.h>
#pragma comment(lib,"Psapi.lib")

#include "MemoryContainer.cpp"
#include "LEDFunc.cpp"

#define defPlayback_SessionName "Playback@GSIOT.gsss.cn"

#define defcheckNetUseable_timeMax (5*60*1000)

//--temptest
#ifdef _DEBUG
//#define defTest_defCfgOprt_GetSelf
//#define defTest_UseRTMFPUrl
#endif

#define  defForceDataSave

static int sg_blUpdatedProc = 0;

// 
struct struLEDShow
{
	std::string name;
	std::string value;
	uint32_t flag;

	struLEDShow()
		: flag(0)
	{
	}

	struLEDShow( const std::string &in_name, const std::string &in_value, const uint32_t in_flag=0 )
		: name(in_name), value(in_value), flag(in_flag)
	{
	}
};

bool operator< ( const struCamAlarmRecv_key &key1, const struCamAlarmRecv_key &key2 )
{
	const int cmpret = key1.sDeviceIP.compare( key2.sDeviceIP );
	if( 0!=cmpret )
	{
		return ( cmpret<0 );
	}

	if( key1.channel != key2.channel )
	{
		return (key1.channel < key2.channel);
	}

	return (key1.nPort < key2.nPort);
}

namespace httpreq
{
	#include "HttpRequest.cpp"
}

std::string g_IOTGetVersion()
{
	return std::string(GSIOT_VERSION);
}

std::string g_IOTGetBuildInfo()
{
	std::string str;
	str += __DATE__;
	str += " ";
	str += __TIME__;
	return str;
}

void PrintLocalIP()
{
	std::list<std::string> LocalIPList;
	std::list<std::string> OtherIP;
	g_GetLocalIP( LocalIPList, OtherIP );

	LOGMSGEX( defLOGNAME, defLOG_INFO, "LocalIP List Begin..." );
	int i = 0;
	for( std::list<std::string>::const_iterator it=LocalIPList.begin(); it!= LocalIPList.end(); ++it )
	{  
		++i;
		LOGMSGEX( defLOGNAME, defLOG_INFO, "LocalIP(%d) %s", i+1, it->c_str() );
	}

	for( std::list<std::string>::const_iterator it=OtherIP.begin(); it!= OtherIP.end(); ++it )
	{  
		++i;
		LOGMSGEX( defLOGNAME, defLOG_INFO, "LocalIP(%d) %s", i+1, it->c_str() );
	}

	LOGMSGEX( defLOGNAME, defLOG_INFO, "LocalIP List End(Count=%d).", i );
}

#ifdef _DEBUG
#define macDebugLog_AlarmGuardState
//#define macDebugLog_AlarmGuardState LOGMSG
#else
#define macDebugLog_AlarmGuardState
#endif

// 当前时间是否在布防有效时间内
bool g_IsValidCurTimeInAlarmGuardState()
{
	// 禁用布防时间功能，布防总是有效
	if( IsRUNCODEEnable(defCodeIndex_TEST_DisableAlarmGuard) )
	{
		macDebugLog_AlarmGuardState( "AlarmGuardTime Disabled!" );

		return true;
	}

	// 当前时间
	SYSTEMTIME st;
	memset( &st, 0, sizeof(st) );
	::GetLocalTime(&st);

	const int agCurTime = st.wHour*100 + st.wMinute;
	const int w = (0==st.wDayOfWeek) ? 7:st.wDayOfWeek;

	// 
	const defCodeIndex_ wIndex = g_AlarmGuardTimeWNum2Index(w);
	if( defCodeIndex_Unknown == wIndex )
	{
		macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: w err invalid", w );
		return false;
	}

	const int ad = RUNCODE_Get(wIndex);
	if( defAlarmGuardTime_AllDay==ad )
	{
		macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: flag fullday valid", w );
		return true;
	}
	else if( defAlarmGuardTime_UnAllDay==ad )
	{
		macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: flag fullday invalid", w );
		return false;
	}
	
	std::vector<uint32_t> vecFlag;
	std::vector<uint32_t> vecBegin;
	std::vector<uint32_t> vecEnd;
	g_GetAlarmGuardTime( wIndex, vecFlag, vecBegin, vecEnd );

	macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: inte: ag1(%d,%d-%d), ag2(%d,%d-%d), ag3(%d,%d-%d)",
		w,
		vecFlag[0], vecBegin[0], vecEnd[0],
		vecFlag[1], vecBegin[1], vecEnd[1],
		vecFlag[2], vecBegin[2], vecEnd[2] );

	//.resetallday
	//if( macAlarmGuardTime_InvaildAG(vecFlag[0],vecBegin[0],vecEnd[0])
	//	&& macAlarmGuardTime_InvaildAG(vecFlag[1],vecBegin[1],vecEnd[1])
	//	&& macAlarmGuardTime_InvaildAG(vecFlag[2],vecBegin[2],vecEnd[2])
	//	)
	//{
	//	macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: all inte valid", w );
	//	return true;
	//}

	// 布防有效时间配置
	for( int i=0; i<defAlarmGuard_AGTimeCount; ++i )
	{
		if( !vecFlag[i] )
			continue;

		const int agTime_Begin = vecBegin[i];
		const int agTime_End = vecEnd[i];

		if( agTime_Begin == agTime_End )
		{
		}
		else if( agTime_Begin < agTime_End )
		{
			if( agCurTime >= agTime_Begin && agCurTime <= agTime_End )
			{
				macDebugLog_AlarmGuardState( "AlarmGuard Time w%d:close inte valid: cur=%d, ag(%d-%d)", w, agCurTime, agTime_Begin, agTime_End );
				return true;
			}
		}
		else
		{
			if( agCurTime >= agTime_Begin || agCurTime <= agTime_End )
			{
				macDebugLog_AlarmGuardState( "AlarmGuard Time w%d:open inte valid: cur=%d, ag(%d-%d)", w, agCurTime, agTime_Begin, agTime_End );
				return true;
			}
		}
	}

	macDebugLog_AlarmGuardState( "AlarmGuard Time w%d: invalid inte: cur=%d", w, agCurTime );
	return false;
}

unsigned __stdcall PlaybackProcThread(LPVOID lpPara)
{
	GSIOTClient *client = (GSIOTClient*)lpPara;

	uint32_t tick = 0;

	CHeartbeatGuard hbGuard( "Playback" );

	while( client->is_running() )
	{
		hbGuard.alive();

		if( !client->PlaybackCmd_OnProc() )
		{
			if( timeGetTime()-tick > 180000 )
			{
				client->Playback_ThreadPrinthb();
				tick = timeGetTime();
			}

			Sleep(10);
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_SYS, "PlaybackProcThread exit." );
	client->OnPlayBackThreadExit();
	return 0;
}

unsigned __stdcall PlayMgrProcThread(LPVOID lpPara)
{
	CoInitialize(NULL);

	GSIOTClient *client = (GSIOTClient*)lpPara;

	uint32_t tick = 0;

	CHeartbeatGuard hbGuard( "PlayMgr" );

	while( client->is_running() )
	{
		hbGuard.alive();

		if( !client->PlayMgrCmd_OnProc() )
		{
			const bool CheckNow = client->PlayMgrCmd_IsCheckNow();
			if( CheckNow
				|| timeGetTime()-tick > 15000 )
			{
				client->PlayMgrCmd_SetCheckNow( false );
				client->GetIPCameraConnection()->CheckRTMPSession( CheckNow );
				Sleep(1000);
				tick = timeGetTime();
			}

			client->check_all_NetUseable( CheckNow );
			client->check_all_devtime();
			client->check_all_devtime_proc();

			Sleep(10);
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_SYS, "PlayMgrProcThread exit." );
	client->OnPlayMgrThreadExit();
	
	CoUninitialize();
	return 0;
}

unsigned __stdcall DataProcThread(LPVOID lpPara)
{
	GSIOTClient *client = (GSIOTClient*)lpPara;
	LOGMSGEX( defLOGNAME, defLOG_SYS, "DataProcThread Running..." );
	
	client->GetDataStoreMgr( true );

	CHeartbeatGuard hbGuard( "DataProc" );

	DWORD dwCheckStat = ::timeGetTime();

	while( client->is_running() )
	{
		hbGuard.alive();

		client->DataSave();
		client->DataProc();
		
		DWORD dwStart = ::timeGetTime();
		while( client->is_running() && ::timeGetTime()-dwStart < 5*1000 )
		{
			Sleep(1);
			client->DataSave();
		}

		if( ::timeGetTime()-dwCheckStat > 60*1000 )
		{
			client->DataStatCheck();
			dwCheckStat = ::timeGetTime();
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_SYS, "DataProcThread exit." );
	client->OnDataProcThreadExit();
	return 0;
}

unsigned __stdcall AlarmProcThread(LPVOID lpPara)
{
	GSIOTClient *client = (GSIOTClient*)lpPara;
	LOGMSGEX( defLOGNAME, defLOG_SYS, "AlarmProcThread Running..." );

	CHeartbeatGuard hbGuard( "AlarmProc" );
	DWORD dwStart = ::timeGetTime();

	while( client->is_running() )
	{
		if( ::timeGetTime()-dwStart > 10000 )
		{
			client->AlarmCheck();
			hbGuard.alive();
			dwStart = ::timeGetTime();
		}

		const bool isdo = client->AlarmProc();

		if( !isdo && client->is_running() )
		{
			Sleep(50);
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_SYS, "AlarmProcThread exit." );
	client->OnAlarmProcThreadExit();
	return 0;
}

unsigned __stdcall ACProcThread( LPVOID lpPara )
{
	GSIOTClient *client = (GSIOTClient*)lpPara;

	uint32_t tick = 0;

	CHeartbeatGuard hbGuard( "ACProc" );

	while( client->is_running() )
	{
		hbGuard.alive();

		if( !client->ACProc() )
		{
			Sleep( 10 );
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_SYS, "ACProcThread exit." );
	client->ACProcThreadExit();

	return 0;
}

struPlaybackSession::struPlaybackSession()
	: dev( NULL )
{
	ts = timeGetTime();
	lastUpdateTS = ts;
}

struPlaybackSession::struPlaybackSession( const std::string &in_key, const std::string &in_from_jid, const std::string &in_url, const std::string &in_peerid, const std::string &in_streamid, const std::string &in_devname, GSIOTDevice *in_dev )
	: key( in_key ), from_jid( in_from_jid ), url( in_url ), peerid( in_peerid ), streamid( in_streamid ), devname( in_devname ), dev( in_dev )
{
	ts = timeGetTime();
	lastUpdateTS = ts;
}

bool struPlaybackSession::check() const
{
	const uint32_t overtime = RUNCODE_Get(defCodeIndex_SYS_PlaybackSessionOvertime);

	if( timeGetTime()-ts > (1000*overtime) )
	{
		LOGMSG( "Playback Session overtime=%ds, devid=%d, devname=%s, url=%s, from=%s", overtime, dev->getId(), dev->getName().c_str(), url.c_str(), from_jid.c_str() );

		return false;
	}

	return true;
}

GSIOTClient::GSIOTClient( IDeviceHandler *handler, const std::string &RunParam )
	: m_parser(this), m_PreInitState(false), m_cfg(NULL), m_event(NULL), timer(NULL), deviceClient(NULL), ipcamClient(NULL),xmppClient(NULL),
	timeCount(0),serverPingCount(0), m_handler(handler), m_running(false), m_IGSMessageHandler(NULL), m_ITriggerDebugHandler(NULL), m_EnableTriggerDebug(false),
	m_last_checkNetUseable_camid(0)
{
	g_SYS_SetGSIOTClient( this );
	this->PreInit( RunParam );

	m_DataStoreMgr = NULL;
}

void GSIOTClient::PreInit( const std::string &RunParam )
{
    CoInitialize(NULL);

	if( !m_RunCodeMgr.get_db() )
	{
		m_PreInitState = false;
		return ;
	}

	m_IOT_starttime = g_GetUTCTime();
	m_str_IOT_starttime = g_TimeToStr( m_IOT_starttime );

	LOGMSGEX( defLOGNAME, defLOG_SYS, "Start Time(%u): %s\r\n", (uint32_t)m_IOT_starttime, m_str_IOT_starttime.c_str() );

	m_cfg = new GSIOTConfig();

	if( !m_cfg->PreInit( RunParam ) )
	{
		m_PreInitState = false;
		return ;
	}

	this->RunCodeInit();

	m_event = new GSIOTEvent();

	ResetNoticeJid();

	APlayer::Init();
	this->m_TalkMgr.set_ITalkNotify( this );

	m_PreInitState = true;
	m_isThreadExit = true;
	m_isPlayBackThreadExit = true;
	m_isPlayMgrThreadExit = true;
	m_PlaybackThreadTick = timeGetTime();
	m_PlaybackThreadCreateCount = 0;

	m_PlayMgr_CheckNowFlag = defPlayMgrCmd_Unknown;

	m_last_checkNetUseable_time = timeGetTime();

	m_isDataProcThreadExit = true;
	m_isAlarmProcThreadExit = true;

	m_lastShowLedTick = timeGetTime();
	m_lastcheck_AC = timeGetTime();
	m_isACProcThreadExit = true;
}

void GSIOTClient::ResetNoticeJid()
{

}

void GSIOTClient::RunCodeInit()
{
	m_RunCodeMgr.Init();
}

void GSIOTClient::Stop(void)
{
	DWORD dwStart = ::timeGetTime();
	m_running = false;
	m_TalkMgr.StopTalk_All();

	if( timer )
	{
		timer->Stop();
	}

	if(deviceClient)
	{
		deviceClient->Stoped();
	}

	if(ipcamClient)
	{
		ipcamClient->Disconnect();
	}

	LOGMSG( "~GSIOTClient: stop other wait usetime=%dms", ::timeGetTime()-dwStart );
	
	dwStart = ::timeGetTime();
	while( ::timeGetTime()-dwStart < 10*1000 )
	{
		if( m_isThreadExit 
			&& m_isPlayBackThreadExit
			&& m_isPlayMgrThreadExit
			&& m_isDataProcThreadExit
			&& m_isAlarmProcThreadExit
			&& m_isACProcThreadExit
			&& (!timer || timer->IsThreadExit() )
			&& (!deviceClient || deviceClient->IsThreadExit() )
			)
		{
			break;
		}

		Sleep(1);
	}

	LOGMSG( "~GSIOTClient: thread exit wait usetime=%dms", ::timeGetTime()-dwStart );
}

GSIOTClient::~GSIOTClient(void)
{
	if(xmppClient){
		xmppClient->disconnect();

	   xmppClient->removeStanzaExtension(ExtIot);
	   xmppClient->removeIqHandler(this, ExtIot);
	   xmppClient->removeStanzaExtension(ExtIotResult);
	   xmppClient->removeIqHandler(this, ExtIotResult);
	   xmppClient->removeStanzaExtension(ExtIotControl);
       xmppClient->removeIqHandler(this, ExtIotControl);
	   xmppClient->removeStanzaExtension(ExtIotHeartbeat);
	   xmppClient->removeIqHandler(this, ExtIotHeartbeat);
	   xmppClient->removeStanzaExtension(ExtIotDeviceInfo);
       xmppClient->removeIqHandler(this, ExtIotDeviceInfo);
	   xmppClient->removeStanzaExtension(ExtIotAuthority);
	   xmppClient->removeIqHandler(this, ExtIotAuthority);
	   xmppClient->removeStanzaExtension(ExtIotAuthority_User);
	   xmppClient->removeIqHandler(this, ExtIotAuthority_User);
	   xmppClient->removeStanzaExtension(ExtIotManager);
	   xmppClient->removeIqHandler(this, ExtIotManager);
	   xmppClient->removeStanzaExtension(ExtIotEvent);
	   xmppClient->removeIqHandler(this, ExtIotEvent);
	   xmppClient->removeStanzaExtension(ExtIotState);
	   xmppClient->removeIqHandler(this, ExtIotState);
	   xmppClient->removeStanzaExtension(ExtIotChange);
	   xmppClient->removeIqHandler(this, ExtIotChange);
	   xmppClient->removeStanzaExtension(ExtIotTalk);
	   xmppClient->removeIqHandler(this, ExtIotTalk);
	   xmppClient->removeStanzaExtension(ExtIotPlayback);
	   xmppClient->removeIqHandler(this, ExtIotPlayback);
	   xmppClient->removeStanzaExtension(ExtIotRelation);
	   xmppClient->removeIqHandler(this, ExtIotRelation);
	   xmppClient->removeStanzaExtension(ExtIotPreset);
	   xmppClient->removeIqHandler(this, ExtIotPreset);
	   xmppClient->removeStanzaExtension(ExtIotVObj);
	   xmppClient->removeIqHandler(this, ExtIotVObj);
	   xmppClient->removeStanzaExtension(ExtIotTrans);
	   xmppClient->removeIqHandler(this, ExtIotTrans);
	   xmppClient->removeStanzaExtension(ExtIotReport);
	   xmppClient->removeIqHandler(this, ExtIotReport);
	   xmppClient->removeStanzaExtension(ExtIotMessage);
	   xmppClient->removeIqHandler(this, ExtIotMessage);
	   xmppClient->removeStanzaExtension(ExtIotUpdate);
	   xmppClient->removeIqHandler(this, ExtIotUpdate);
	   xmppClient->removeSubscriptionHandler(this);
	   xmppClient->removeMessageHandler(this);
	   xmppClient->removeIqHandler(this,ExtPing);
	   delete(xmppClient);
	}
	
	PlaybackCmd_clean();
	Playback_DeleteAll();

	if(ipcamClient){
		//ipcamClient->Disconnect();
		delete(ipcamClient);
	}

	if(deviceClient){
		//deviceClient->Stoped();
		delete(deviceClient);
	}

	if( m_cfg ) delete(m_cfg);
    if( m_event ) delete(m_event);
	if( timer ) delete(timer);

	if( m_DataStoreMgr )
	{
		delete m_DataStoreMgr;
		m_DataStoreMgr = NULL;
	}

	//FinalClearControlMesssageQueue();//...testFinalDelete

	//g_DelAllMemoryContainer();//...testFinalDelete

	CoUninitialize();
}

void GSIOTClient::MemoryContainer_PrintState( bool printall, const char *pinfo )
{
	g_GetMemoryContainer()->PrintState( printall, pinfo );

	////g_GetMemoryContainer()->PrintState( false, "checkBefore" );
	//if( g_GetMemoryContainer()->CheckMemory() )
	//{
	//	g_GetMemoryContainer()->PrintState( false, "checkAfter" );
	//}
}

void GSIOTClient::CheckSystem()
{
	if( !IsRUNCODEEnable(defCodeIndex_SYS_CheckSystem) )
	{
		return;
	}

	const uint32_t DIV = 1024*1024;
	MEMORYSTATUSEX statex = {0};
	statex.dwLength = sizeof (statex);
	GlobalMemoryStatusEx (&statex);

	const uint32_t cur_sys_mem_p = statex.dwMemoryLoad;
	const uint32_t SYS_CurSysMemOver = RUNCODE_Get(defCodeIndex_SYS_CurSysMemOver);
	const uint32_t SYS_CurSysMemOverLow = RUNCODE_Get(defCodeIndex_SYS_CurSysMemOverLow);

	SYSTEMTIME SystemTime;
	memset( &SystemTime, 0, sizeof(SystemTime) );
	GetLocalTime( &SystemTime );

	if( cur_sys_mem_p >= SYS_CurSysMemOver )
	{
		LOGMSG( "cur sys mem: %d/100 use, limit=%d, %d MB total of physical memory, cur over sys_reset now\r\n", cur_sys_mem_p, SYS_CurSysMemOver, (DWORD)(statex.ullTotalPhys/DIV) );

		sys_reset( "SYS_CurSysMemOver", 1 );
	}
	else if( (cur_sys_mem_p >= SYS_CurSysMemOverLow) && (SystemTime.wHour >= 1 && SystemTime.wHour < 4) )
	{
		LOGMSG( "cur sys mem: %d/100 use, limit=%d, %d MB total of physical memory, cur low over(%d) & timehour(1-4) sys_reset now\r\n", cur_sys_mem_p, SYS_CurSysMemOver, (DWORD)(statex.ullTotalPhys/DIV), SYS_CurSysMemOverLow );

		sys_reset( "SYS_CurSysMemOverLow", 1 );
	}
	else
	{
		LOGMSG( "cur sys mem: %d/100 use, limit=%d, %d MB total of physical memory", cur_sys_mem_p, SYS_CurSysMemOver, (DWORD)(statex.ullTotalPhys/DIV) );
	}
}

void GSIOTClient::CheckIOTPs()
{
	if( !IsRUNCODEEnable(defCodeIndex_SYS_CheckIOTPs) )
	{
		return;
	}

	const DWORD tick = timeGetTime();

	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof(pmc) );

	const uint32_t cur_ps_mem = pmc.WorkingSetSize/(1000*1000);
	const uint32_t c_SYS_CurPsMemOver = RUNCODE_Get(defCodeIndex_SYS_CurPsMemOver);
	const uint32_t c_SYS_CurPsMemOver_low = c_SYS_CurPsMemOver*80/100;

	SYSTEMTIME SystemTime;
	memset( &SystemTime, 0, sizeof(SystemTime) );
	GetLocalTime( &SystemTime );

	if( cur_ps_mem >= c_SYS_CurPsMemOver )
	{
		LOGMSG( "cur ps mem: %d MB, limit=%d, gettick=%dms, cur over FastRestart now\r\n", cur_ps_mem, c_SYS_CurPsMemOver, timeGetTime()-tick );
		
		if( m_IGSMessageHandler )
			m_IGSMessageHandler->OnGSMessage( defGSMsgType_FastRestart, 0 );
	}
	else if( (cur_ps_mem >= c_SYS_CurPsMemOver_low) && (SystemTime.wHour >= 1 && SystemTime.wHour < 4) )
	{
		LOGMSG( "cur ps mem: %d MB, limit=%d, gettick=%dms, cur 80%% over & timehour(1-4) FastRestart now\r\n", cur_ps_mem, c_SYS_CurPsMemOver, timeGetTime()-tick );

		if( m_IGSMessageHandler )
			m_IGSMessageHandler->OnGSMessage( defGSMsgType_FastRestart, 0 );
	}
	else
	{
		LOGMSG( "cur ps mem: %d MB, limit=%d, gettick=%dms\r\n", cur_ps_mem, c_SYS_CurPsMemOver, timeGetTime()-tick );
	}
}

void GSIOTClient::OnTimeOverForCmdRecv( const defLinkID LinkID, const IOTDeviceType DevType, const uint32_t DevID, const uint32_t addr )
{
	if( IOT_DEVICE_Unknown==DevType || 0==DevID )
	{
		return;
	}

	switch( DevType )
	{
	case IOT_DEVICE_RS485:
		{
			std::list<GSIOTDevice*>::const_iterator it = IotDeviceList.begin();
			std::list<GSIOTDevice*>::const_iterator itEnd = IotDeviceList.end();
			for( ; it!=itEnd && addr; ++it )
			{
				GSIOTDevice *pCurDev = (*it);
				if( pCurDev->getType() != DevType )
					continue;

				if( pCurDev->getId() != DevID )
					continue;

				if( pCurDev->GetLinkID() != LinkID )
					return;

				RS485DevControl *pCurCtl = (RS485DevControl*)pCurDev->getControl();
				if( !pCurCtl )
					return;

				DeviceAddress *pCurAddr = pCurCtl->GetAddress( addr );
				if( pCurAddr )
				{
					bool isChanged = false;
					pCurCtl->check_NetUseable_RecvFailed( &isChanged );

					if( isChanged && m_handler )
					{
						//m_handler->OnDeviceNotify( defDeviceNotify_StateChanged, pCurDev, pCurAddr );
					}
				}

				return;
			}
		}
		break;
	}
}

defUseable GSIOTClient::get_all_useable_state_ForLinkID( defLinkID LinkID )
{
	if( defLinkID_Local == LinkID )
	{
		return defUseable_OK;//return deviceClient->GetPortState()>0 ? defUseable_OK:defUseable_Err;
	}

	CCommLinkAuto_Run_Info_Get AutoCommLink( deviceClient->m_CommLinkMgr, LinkID );
	CCommLinkRun *pCommLink = AutoCommLink.p();
	if( pCommLink )
	{
		return pCommLink->get_all_useable_state_ForDevice();
	}

	return defUseable_Err;
}

void GSIOTClient::OnDeviceDisconnect(GSIOTDevice *iotdevice)
{
	std::list<GSIOTDevice *>::const_iterator it = IotDeviceList.begin();
	for(;it!=IotDeviceList.end();it++){
		if((*it)->getId() == iotdevice->getId() && (*it)->getType() == iotdevice->getType()){
			IotDeviceList.erase(it);
			break;
		}
	}
	OnDeviceStatusChange();
	if(m_handler){
		m_handler->OnDeviceDisconnect(iotdevice);
	}
}

void GSIOTClient::OnDeviceConnect(GSIOTDevice *iotdevice)
{
	std::list<GSIOTDevice *>::const_iterator it = IotDeviceList.begin();
	for(;it!=IotDeviceList.end();it++){
		if((*it)->getId() == iotdevice->getId() && (*it)->getType() == iotdevice->getType()){
		    return;
		}
	}
	IotDeviceList.push_back(iotdevice);
	OnDeviceStatusChange();
	if(m_handler){
		m_handler->OnDeviceConnect(iotdevice);
	}
}

void GSIOTClient::OnDeviceNotify( defDeviceNotify_ notify, GSIOTDevice *iotdevice, DeviceAddress *addr )
{
	if( defDeviceNotify_Modify == notify )
	{
		std::list<GSIOTDevice*>::const_iterator it = IotDeviceList.begin();
		for(;it!=IotDeviceList.end();it++)
		{
			GSIOTDevice *pDev = (*it);
			if( pDev->getId() == iotdevice->getId()
				&& pDev->getType() == iotdevice->getType() )
			{
				if( addr )
				{
					// 只更新地址
					ControlBase *pCCtl = pDev->getControl();
					switch(pCCtl->GetType())
					{
					case IOT_DEVICE_RS485:
						{
							RS485DevControl *ctl = (RS485DevControl*)pCCtl;
							ctl->UpdateAddress( addr );
						}
						break;
					}
				}
				else
				{
					// 只更新设备
					pDev->setName( iotdevice->getName() );
				}

				break;
			}
		}
	}

	if( m_handler )
	{
		m_handler->OnDeviceNotify( notify, iotdevice, addr );
	}
}

void GSIOTClient::OnDeviceData( defLinkID LinkID, GSIOTDevice *iotdevice, ControlBase *ctl, GSIOTObjBase *addr )
{
	const int thisThreadId = ::GetCurrentThreadId();

	LOGMSG( "OnDeviceData Link%d, ctl(%d,%d)-ThId%d", LinkID, ctl->GetType(), iotdevice?iotdevice->getId():0, thisThreadId );

	switch(ctl->GetType())
	{
	case IOT_DEVICE_RS485:
		{
			const bool hasaddr = ( addr && ((DeviceAddress*)addr)->GetAddress()>0 );

			if( !hasaddr )
			{
				RS485DevControl *rsctl = (RS485DevControl*)ctl;

				const defAddressQueue &AddrQue = rsctl->GetAddressList();
				if( !AddrQue.empty() )
				{
					defAddressQueue::const_iterator itAddrQue = AddrQue.begin();
					for( ; itAddrQue!=AddrQue.end(); ++itAddrQue )
					{
						DeviceAddress *pOneAddr = *itAddrQue;

						OnDeviceData_ProcOne( LinkID, iotdevice, ctl, pOneAddr );
					}

					return;
				}
			}
		}
	}

	OnDeviceData_ProcOne( LinkID, iotdevice, ctl, (DeviceAddress*)addr );
}

void GSIOTClient::OnDeviceData_ProcOne( defLinkID LinkID, GSIOTDevice *iotdevice, ControlBase *ctl, DeviceAddress *addr )
{
	const int thisThreadId = ::GetCurrentThreadId();
	const time_t curUTCTime = g_GetUTCTime();

	switch(ctl->GetType())
	{
	case IOT_DEVICE_Trigger:
		{
			TriggerControl *tctl = (TriggerControl *)ctl;

			struGSTime curdt;
			g_struGSTime_GetCurTime( curdt );

			if( m_EnableTriggerDebug && m_ITriggerDebugHandler )
			{
				m_ITriggerDebugHandler->OnTriggerDebug( LinkID, iotdevice?iotdevice->getType():IOT_DEVICE_Unknown, iotdevice?iotdevice->getName():"", tctl->GetAGRunState(), this->GetAlarmGuardGlobalFlag(), g_IsValidCurTimeInAlarmGuardState(), curdt, iotdevice->GetStrAlmBody( true, curdt ), iotdevice->GetStrAlmSubject( true ) );
			}

			tctl->CompareTick();
			if(tctl->IsTrigger(true)){

				LOGMSG( "TriggerControl(id=%d,name=%s) isTrigger true, CurTriggerCount=%d -ThId%d\r\n",
					iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"", tctl->GetCurTriggerCount(), thisThreadId );

				const int AlarmGuardGlobalFlag = this->GetAlarmGuardGlobalFlag();
				const bool IsValidCurTime = g_IsValidCurTimeInAlarmGuardState();

				this->DoAlarmDevice( iotdevice, tctl->GetAGRunState(), AlarmGuardGlobalFlag, IsValidCurTime, iotdevice->GetStrAlmBody( true, curdt ), iotdevice->GetStrAlmSubject( true ) );

				tctl->SetTriggerDo();
			}
		    break;
		}
	case IOT_DEVICE_Remote:
		{
			break;
		}
	case IOT_DEVICE_RFDevice:
		{
			break;
		}
	case IOT_DEVICE_CANDevice:
		{
			break;
		}
	case IOT_DEVICE_RS485:
		{
#if 1
			// update device cur value
			std::list<GSIOTDevice*>::const_iterator it = IotDeviceList.begin();
			std::list<GSIOTDevice*>::const_iterator itEnd = IotDeviceList.end();
			for( ; it!=itEnd && addr; ++it )
			{
				GSIOTDevice *pCurDev = NULL;
				RS485DevControl *pCurCtl = NULL;
				DeviceAddress *pCurAddr = NULL;

				pCurDev = (*it);

				if( !pCurDev->GetEnable() )
					continue;

				pCurCtl = (RS485DevControl*)pCurDev->getControl();

				if( !pCurCtl )
					continue;

				if( pCurDev->GetLinkID() != LinkID )
					continue;

				if( iotdevice )
				{
					if( !GSIOTClient::Compare_Device( iotdevice, pCurDev ) )
						continue;
				}

				if( !GSIOTClient::Compare_Control( pCurCtl, ctl ) )
					continue;

				pCurAddr = pCurCtl->GetAddress( addr->GetAddress() );

				if( !pCurAddr )
					continue;

				if( !pCurAddr->GetEnable() )
					continue;

				if( pCurAddr )
				{
					std::string strlog;
					if( !pCurAddr->SetCurValue( addr->GetCurValue(), curUTCTime, true, &strlog ) )
					{
						if( !strlog.empty() )
						{
							LOGMSG( "%s dev(%d,%d)-ThId%d", strlog.c_str(), pCurDev->getType(), pCurDev->getId(), thisThreadId );
						}

						continue;
					}

					bool isChanged = false;
					pCurCtl->set_NetUseable( defUseable_OK, &isChanged );

					if( m_DataStoreMgr && g_isNeedSaveType(pCurAddr->GetType()) )
					{
						bool doSave = false;
						time_t SaveTime = 0;
						std::string SaveValue = "0";
						defDataFlag_ dataflag = defDataFlag_Norm;
						strlog = "";

						pCurAddr->DataAnalyse( addr->GetCurValue(), curUTCTime, &doSave, &SaveTime, &SaveValue, &dataflag, &strlog );

#if defined(defForceDataSave)
						if( IsRUNCODEEnable(defCodeIndex_TEST_ForceDataSave) )
						{
							if( !doSave )
							{
								doSave = true;
								SaveTime = curUTCTime;
								SaveValue = addr->GetCurValue();
								strlog = "force save";
							}
						}
#endif

						if( !strlog.empty() )
						{
							LOGMSG( "%s dev(%d,%d)-ThId%d", strlog.c_str(), pCurDev->getType(), pCurDev->getId(), thisThreadId );
						}

						if( doSave )
						{
							gloox::util::MutexGuard mutexguard( m_mutex_DataStore );

							const size_t DataSaveBufSize = m_lstDataSaveBuf.size();
							if( DataSaveBufSize<10000 )
							{
								m_lstDataSaveBuf.push_back( new struDataSave( SaveTime, pCurDev->getType(), pCurDev->getId(), pCurAddr->GetType(), pCurAddr->GetAddress(), dataflag, SaveValue, pCurDev->getName() + "-" + pCurAddr->GetName() ) );
							}
							else if( DataSaveBufSize > 100 )
							{
								LOGMSG( "lstDataSaveBuf max, size=%d -ThId%d", m_lstDataSaveBuf.size(), thisThreadId );
							}
							//m_DataStoreMgr->insertdata( SaveTime, pCurDev->getType(), pCurDev->getId(), pCurAddr->GetType(), pCurAddr->GetAddress(), dataflag, SaveValue, pCurDev->getName() + "-" + pCurAddr->GetName() );
						}
					}

					if( isChanged && m_handler )
					{
						//m_handler->OnDeviceNotify( defDeviceNotify_StateChanged, pCurDev, pCurAddr );
					}
				}
			}
#endif
		}
		break;
	}
	//传送给UI
	if(m_handler){
	    m_handler->OnDeviceData(LinkID, iotdevice, ctl, addr);
	}

	while(1)
	{
		ControlMessage *pCtlMsg = PopControlMesssageQueue( iotdevice, ctl, addr );
		if( pCtlMsg && addr )
		{
			DeviceAddress *reAddr = (DeviceAddress*)pCtlMsg->GetObj();
			reAddr->SetCurValue(addr->GetCurValue());

			if( pCtlMsg->GetDevice() )
			{
				pCtlMsg->GetDevice()->SetCurValue( addr );
			}

			if( pCtlMsg->GetJid() )
			{
			IQ re( IQ::Result, pCtlMsg->GetJid(), pCtlMsg->GetId());
			re.addExtension(new GSIOTControl(pCtlMsg->GetDevice()->clone(), defUserAuth_RW, false));
			XmppClientSend(re,"OnDeviceData Send");
			}

			delete pCtlMsg;
		}
		else
		{
			break;
		}
	}
}

void GSIOTClient::DoAlarmDevice( const GSIOTDevice *iotdevice, const bool AGRunState, const int AlarmGuardGlobalFlag, const bool IsValidCurTime, const std::string &strAlmBody, const std::string &strAlmSubject )
{
	if( !iotdevice )
		return ;

	LOGMSG( "DoAlarmDevice Begin(id=%d,name=%s) AGRunState=%d", iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"", (int)AGRunState );

	if( !AlarmGuardGlobalFlag )
	{
		LOGMSG( "DoAlarmDevice(id=%d,name=%s) AlarmGuardGlobalFlag=%d is stop\r\n",
			iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"", AlarmGuardGlobalFlag );
		//break;
	}
	else if( !AGRunState )
	{
		LOGMSG( "DoAlarmDevice(id=%d,name=%s) AGRunState=%d is stop run\r\n",
			iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"", (int)AGRunState );
		//break;
	}
	else if( !IsValidCurTime )
	{
		LOGMSG( "DoAlarmDevice(id=%d,name=%s) AGRunState=%d is running, but AlarmGuard invalid time\r\n",
			iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"", (int)AGRunState );
		//break;
	}

	std::list<ControlEvent *> evtList = m_event->GetEvents();
	std::list<ControlEvent *>::const_iterator it = evtList.begin();
	for(;it!=evtList.end();it++)
	{
		if((*it)->GetEnable() 
			&& (*it)->GetDeviceType() == iotdevice->getType()
			&& (*it)->GetDeviceID() == iotdevice->getId()
			)
		{
			if( (*it)->isForce()
				|| ( AlarmGuardGlobalFlag && AGRunState && IsValidCurTime ) // 布防时执行
				)	
			{
				// 触发动作
				bool needDoInterval = DoControlEvent( iotdevice->getType(), iotdevice->getId(), (*it), true, strAlmBody, strAlmSubject, (*it)->isForce()?"Force Trigger doevent":"Trigger doevent" );

				if( needDoInterval )
				{
					uint32_t DoInterval = (*it)->GetDoInterval();
					DoInterval = DoInterval>3000 ? 3000:DoInterval;

					if( DoInterval > 0 )
					{
						Sleep( DoInterval );
					}
				}
			}
		}
	}

	LOGMSG( "DoAlarmDevice End(id=%d,name=%s)", iotdevice?iotdevice->getId():0, iotdevice?iotdevice->getName().c_str():"" );
}

bool GSIOTClient::DoControlEvent( const IOTDeviceType DevType, const uint32_t DevID, ControlEvent *ctlevt, const bool isAlarm, const std::string &strAlmBody, const std::string &strAlmSubject, const char *callinfo, const bool isTest, const char *teststr )
{
	bool needDoInterval = false;

	if( !isTest )
	{
		uint32_t outDoInterval = 0;
		if( !ctlevt->IsCanDo(m_cfg, outDoInterval) )
		{
			LOGMSG( "DoControlEvent IsCanDo=false, evttype=%d, evtid=%d, DoInterval=%d", ctlevt->GetType(), ctlevt->GetID(), outDoInterval );

			return needDoInterval;
		}

		ctlevt->SetDo();
	}

	switch(ctlevt->GetType()){
	case SMS_Event:
		{
			AutoSendSMSEvent *pnewEvt = (AutoSendSMSEvent*)((AutoSendSMSEvent*)ctlevt)->clone();
			pnewEvt->SetTest( isTest );

			if( pnewEvt->GetSMS().empty() )
			{
				pnewEvt->SetSMS( strAlmBody );
			}

			deviceClient->GetGSM().AddSMS( pnewEvt );
		}
		break;

	case EMAIL_Event:
		//未实现
		break;

	case NOTICE_Event:
		{
			AutoNoticeEvent *aEvt = (AutoNoticeEvent *)ctlevt;

			//JID to_jid;
			//to_jid.setJID( aEvt->GetToJid() );

			std::set<std::string> jidlist;
			if( aEvt->GetToJid().empty() )
			{
				//if( !m_cfg->GetNoticeJid().empty() )
				//{
				//	jidlist.insert( m_cfg->GetNoticeJid() );
				//}
				
				const defmapGSIOTUser &mapUser = m_cfg->m_UserMgr.GetList_User();
				for( defmapGSIOTUser::const_iterator it=mapUser.begin(); it!=mapUser.end(); ++it )
				{
					const GSIOTUser *pUser = it->second;

					if( !pUser->GetEnable() )
						continue;

					if( !pUser->get_UserFlag( defUserFlag_NoticeGroup ) )
						continue;

					const defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, ctlevt->GetDeviceType(), ctlevt->GetDeviceID() );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
						continue;

					jidlist.insert( pUser->GetJid() );
				}
			}
			else
			{
				jidlist.insert( aEvt->GetToJid() );
			}

			//if( m_NoticeJid || to_jid )
			if( !jidlist.empty() )
			{
				std::string strBody = aEvt->GetBody();
				if( strBody.empty() )
				{
					strBody = strAlmBody;
				}

				std::string strSubject = aEvt->GetSubject();
				if( strSubject.empty() )
				{
					strSubject = strAlmSubject;
				}

				if( isTest && teststr )
				{
					strBody = std::string(teststr) + strBody;
					//strBody += " (本条消息为系统测试)";
				}

				XmppClientSend_jidlist( jidlist, strBody, strSubject, callinfo );
			}
			else
			{
				LOGMSGEX( defLOGNAME, defLOG_WORN, "NoticeJid is invalid! no noticejid\r\n" );
			}
		}
		break;

	case CONTROL_Event:
		{
			AutoControlEvent *aEvt = (AutoControlEvent *)ctlevt;
			GSIOTDevice* ctldev = this->GetIOTDevice( aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId() );
			ControlBase *cctl = ctldev ? ctldev->getControl() : NULL;
			if(cctl){
				switch(cctl->GetType()){
				case IOT_DEVICE_Remote:
					{
						GSIOTDevice *sendDev = ctldev->clone( false );
						RFRemoteControl *ctl = (RFRemoteControl*)sendDev->getControl();
						RemoteButton *pButton = ctl->GetButton(aEvt->GetAddress());
						if(pButton)
						{
							ctl->ButtonQueueChangeToOne( pButton->GetId() );
							ctl->Print( callinfo, true, pButton );
							this->SendControl(DevType, sendDev, NULL, defNormSendCtlOvertime, defNormMsgOvertime, aEvt->GetDoInterval()>0 ? aEvt->GetDoInterval():1 );
						}
						macCheckAndDel_Obj(ctl);
						break;
					}
					break;
				case IOT_DEVICE_RFDevice:
					{
						//RFDeviceControl *rfCtl = (RFDeviceControl *)cctl;
						//RFDevice *dev = rfCtl->GetDevice();
						//addr = dev->GetAddress(aEvt->GetAddress());
						//if(addr){
						//	rfCtl->SetCommand(Device_Write_Request);
						//	addr->SetCurValue(aEvt->GetValue());
						//}
						break;
					}
				case IOT_DEVICE_CANDevice:
					{
						//CANDeviceControl *canCtl = (CANDeviceControl *)cctl;
						//addr = canCtl->GetAddress(aEvt->GetAddress());
						//if(addr){
						//	canCtl->SetCommand(CAN_Address_Write_Request);
						//	addr->SetCurValue(aEvt->GetValue());
						//}
						break;
					}
				case IOT_DEVICE_RS485:
					{
						GSIOTDevice *sendDev = ctldev->clone( false );
						RS485DevControl *rsCtl = (RS485DevControl*)sendDev->getControl();
						DeviceAddress *addr = rsCtl->GetAddress(aEvt->GetAddress());
						if(addr)
						{
							rsCtl->SetCommand( defModbusCmd_Write );
							addr->SetCurValue( aEvt->GetValue() );

							if(addr)
							{
								rsCtl->Print( callinfo, true, addr );
								this->SendControl(DevType, sendDev, addr, defNormSendCtlOvertime, defNormMsgOvertime, aEvt->GetDoInterval()>0 ? aEvt->GetDoInterval():1 );
							}
						}
						macCheckAndDel_Obj(rsCtl);
						break;
					}

				case IOT_DEVICE_Camera:
					{
						IPCameraBase *ctl = (IPCameraBase*)cctl;
						if( ctl->GetAdvAttr().get_AdvAttr( defCamAdvAttr_PTZ_Preset ) )
						{
							const CPresetObj *pPresetLocal = ctl->GetPreset(aEvt->GetAddress());
							if( pPresetLocal )
							{
								ctl->SendPTZ( GSPTZ_Goto_Preset, pPresetLocal->GetIndex(), 0, 0, callinfo );
								needDoInterval = true;
							}
						}
					}
					break;
				}
			}
			break;
		}

	case Eventthing_Event:
		{
			AutoEventthing *aEvt = (AutoEventthing *)ctlevt;
			DoControlEvent_Eventthing( aEvt, ctlevt, callinfo, isTest );
		}
		break;
	}

	return needDoInterval;
}

void GSIOTClient::DoControlEvent_Eventthing( const AutoEventthing *aEvt, const ControlEvent *ctlevt, const char *callinfo, const bool isTest )
{
	if( aEvt->IsAllDevice() )
	{
		LOGMSG( "Do Eventthing_Event all dev set runstate=%d", aEvt->GetRunState() );

		SetAlarmGuardGlobalFlag( aEvt->GetRunState() ); // this->SetAllEventsState( aEvt->GetRunState(), callinfo, false );

		//const std::list<GSIOTDevice*> devices = deviceClient->GetDeviceManager()->GetDeviceList();
		//for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
		//{
		//	if( IOT_DEVICE_Trigger == (*it)->getType() )
		//	{
		//		DoControlEvent_Eventthing_Event( (GSIOTDevice*)(*it), aEvt, ctlevt, callinfo, isTest );
		//	}
		//}
	}
	else
	{
		LOGMSG( "Do Eventthing_Event devtype=%d, devid=%d, set runstate=%d", aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId(), aEvt->GetRunState() );

		GSIOTDevice *dev = deviceClient->GetIOTDevice( aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId() );
		DoControlEvent_Eventthing_Event( dev, aEvt, ctlevt, callinfo, isTest );
	}
}

void GSIOTClient::DoControlEvent_Eventthing_Event( GSIOTDevice *dev, const AutoEventthing *aEvt, const ControlEvent *ctlevt, const char *callinfo, const bool isTest )
{
	if( dev )
	{
		if( dev->getControl() )
		{
			switch( dev->getType() )
			{
			case IOT_DEVICE_Trigger:
				{
					TriggerControl *ctl = (TriggerControl *)dev->getControl();
					ctl->SetAGRunState( aEvt->GetRunState() );
					deviceClient->ModifyDevice( dev, 0 );
				}
				break;

			case IOT_DEVICE_Camera:
				{
					IPCameraBase *ctl = (IPCameraBase*)dev->getControl();
					ctl->SetAGRunState( aEvt->GetRunState() );
					deviceClient->ModifyDevice( dev, 0 );
				}
				break;

			default:
				LOGMSG( "Do Eventthing_Event devtype=%d, devid=%d, not support", aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId() );
				break;
			}
		}
		else
		{
			LOGMSG( "Do Eventthing_Event found dev and ctl err, devtype=%d, devid=%d", aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId() );
		}
	}
	else
	{
		LOGMSG( "Do Eventthing_Event not found dev, devtype=%d, devid=%d", aEvt->GetControlDeviceType(), aEvt->GetControlDeviceId() );
	}
}

GSAGCurState_ GSIOTClient::GetAllEventsState() const
{
	bool isAllTrue = true;
	bool isAllFalse = true;
	
	std::list<GSIOTDevice*> devices = deviceClient->GetDeviceManager()->GetDeviceList();
	for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
	{
		const GSIOTDevice* dev = (*it);

		if( !dev->getControl() )
			continue;

		if( !dev->GetEnable()
			|| !GSIOTDevice::IsSupportAlarm(dev)
			)
		{
			continue;
		}

		bool AGRunState = false;
		switch( dev->getType() )
		{
		case IOT_DEVICE_Trigger:
			{
				const TriggerControl *ctl = (TriggerControl*)dev->getControl();
				AGRunState = ctl->GetAGRunState();
			}
			break;

		case IOT_DEVICE_Camera:
			{
				const IPCameraBase *ctl = (IPCameraBase*)dev->getControl();
				AGRunState = ctl->GetAGRunState();
			}
			break;

		default:
			continue;
		}

		if( AGRunState )
		{
			isAllFalse = false;
		}
		else
		{
			isAllTrue = false;
		}
	}

	devices = ipcamClient->GetCameraManager()->GetCameraList();
	for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
	{
		const GSIOTDevice* dev = (*it);

		if( !dev->getControl() )
			continue;

		if( !dev->GetEnable()
			|| !GSIOTDevice::IsSupportAlarm(dev)
			)
		{
			continue;
		}

		bool AGRunState = false;
		switch( dev->getType() )
		{
		case IOT_DEVICE_Trigger:
			{
				const TriggerControl *ctl = (TriggerControl*)dev->getControl();
				AGRunState = ctl->GetAGRunState();
			}
			break;

		case IOT_DEVICE_Camera:
			{
				const IPCameraBase *ctl = (IPCameraBase*)dev->getControl();
				AGRunState = ctl->GetAGRunState();
			}
			break;

		default:
			continue;
		}

		if( AGRunState )
		{
			isAllFalse = false;
		}
		else
		{
			isAllTrue = false;
		}
	}

	if( isAllTrue )
		return GSAGCurState_AllArmed;	// 全部运行

	if( isAllFalse )
		return GSAGCurState_UnArmed;	// 不工作

	return GSAGCurState_PartOfArmed;	// 部分工作
}

void GSIOTClient::SetAllEventsState( const bool AGRunState, const char *callinfo, const bool forcesave )
{
	LOGMSG( "SetAllEventsState=%d, forcesave=%d, info=%s\r\n", (int)AGRunState, (int)forcesave, callinfo?callinfo:"" );

	SetAllEventsState_do( AGRunState, forcesave, true );
	SetAllEventsState_do( AGRunState, forcesave, false );
}

void GSIOTClient::SetAllEventsState_do( const bool AGRunState, const bool forcesave, bool isEditCam )
{
	SQLite::Database *db = isEditCam ? ipcamClient->GetCameraManager()->get_db() : deviceClient->GetDeviceManager()->get_db();
	UseDbTransAction dbta(db);

	const std::list<GSIOTDevice*> devices = isEditCam ? ipcamClient->GetCameraManager()->GetCameraList() : deviceClient->GetDeviceManager()->GetDeviceList();

	for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
	{
		GSIOTDevice* dev = (*it);

		if( isEditCam )
		{
			if( IOT_DEVICE_Camera != dev->getType() )
				continue;
		}
		else
		{
			if( IOT_DEVICE_Camera == dev->getType() )
				continue;
		}

		if( !dev->getControl() )
			continue;

		if( !dev->GetEnable()
			|| !GSIOTDevice::IsSupportAlarm(dev)
			)
		{
			continue;
		}

		switch( dev->getType() )
		{
		case IOT_DEVICE_Trigger:
			{
				TriggerControl *ctl = (TriggerControl*)dev->getControl();
				if( forcesave || ctl->GetAGRunState() != AGRunState )
				{
					ctl->SetAGRunState( AGRunState );
				}
			}
			break;

		case IOT_DEVICE_Camera:
			{
				IPCameraBase *ctl = (IPCameraBase*)dev->getControl();
				if( forcesave || ctl->GetAGRunState() != AGRunState )
				{
					ctl->SetAGRunState( AGRunState );
				}
			}
			break;

		default:
			continue;
		}

		if( isEditCam )
		{
			ipcamClient->ModifyDevice( dev );
		}
		else
		{
			deviceClient->ModifyDevice( dev );
		}
	}
}

void GSIOTClient::SetIGSMessageHandler( IGSMessageHandler *handler )
{
	this->m_IGSMessageHandler = handler;
}

void GSIOTClient::SetITriggerDebugHandler( ITriggerDebugHandler *handler )
{
	this->m_ITriggerDebugHandler = handler;
}

void GSIOTClient::AddGSMessage( GSMessage *pMsg )
{
	if( !pMsg )
		return;

	defGSMsgType_ MsgType = pMsg->getMsgType();

	m_mutex_lstGSMessage.lock();

	// 缓存大于此值时开始进行释放，释放掉一定量
	if( m_lstGSMessage.size()>10000 )
	{
		LOGMSGEX( defLOGNAME, defLOG_WORN, "AddGSMessage full, do release , beforeCount=%u\r\n", m_lstGSMessage.size() );

		while( m_lstGSMessage.size()>500 )
		{
			GSMessage *p =  m_lstGSMessage.front();
			m_lstGSMessage.pop_front();
			delete p;
		}

		LOGMSGEX( defLOGNAME, defLOG_WORN, "AddGSMessage full, do release , AfterCount=%u\r\n", m_lstGSMessage.size() );
	}

	m_lstGSMessage.push_back( pMsg );

	m_mutex_lstGSMessage.unlock();

	if( m_IGSMessageHandler )
		m_IGSMessageHandler->OnGSMessage( MsgType, 0 );
}

GSMessage* GSIOTClient::PopGSMessage()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstGSMessage );

	if( m_lstGSMessage.size()>0 )
	{
		GSMessage *p = m_lstGSMessage.front();
		m_lstGSMessage.pop_front();
		return p;
	}

	return NULL;
}

void GSIOTClient::OnGSMessageProcess()
{
	DWORD dwStart = ::timeGetTime();
	while( ::timeGetTime()-dwStart < 700 )
	{
		GSMessage *pMsg = PopGSMessage();
		if( !pMsg )
			return ;

		if( pMsg->isOverTime() )
		{
			LOGMSG( "OnGSMessageProcess overtime!!! MsgType=%d,", pMsg->getMsgType() );
			delete pMsg;
			return;
		}

		LOGMSG( "OnGSMessageProcess MsgType=%d,", pMsg->getMsgType() );

		if( pMsg->getpEx() )
		{
			switch(pMsg->getpEx()->extensionType())
			{
			case ExtIotAuthority:
				{
					handleIq_Set_XmppGSAuth( pMsg );
				}
				break;

			case ExtIotManager:
				{
					handleIq_Set_XmppGSManager( pMsg );
				}
				break;

			case ExtIotEvent:
				{
					handleIq_Set_XmppGSEvent( pMsg );
				}
				break;

			case ExtIotRelation:
				{
					handleIq_Set_XmppGSRelation( pMsg );
				}
				break;

			case ExtIotPreset:
				{
					handleIq_Set_XmppGSPreset( pMsg );
				}
				break;

			case ExtIotVObj:
				{
					handleIq_Set_XmppGSVObj( pMsg );
				}
				break;

			default:
				{
					LOGMSGEX( defLOGNAME, defLOG_ERROR, "OnGSMessageProcess MsgType=%d, exType=%d err", pMsg->getMsgType(), pMsg->getpEx()->extensionType() );
				}
				break;
			}
		}

		delete pMsg;
	}
}

void GSIOTClient::PushControlMesssageQueue( ControlMessage *pCtlMsg )
{
	gloox::util::MutexGuard mutexguard( m_mutex_ctlMessageList );

	ctlMessageList.push_back( pCtlMsg );

	// 缓存大于此值时开始进行释放，释放掉一定量
	if( ctlMessageList.size()>10000  )
	{
		LOGMSGEX( defLOGNAME, defLOG_WORN, "PushControlMesssageQueue full, do release , beforeCount=%u\r\n", ctlMessageList.size() );

		while( ctlMessageList.size()>500 )
		{
			ControlMessage *p =  ctlMessageList.front();
			ctlMessageList.pop_front();
			delete p;
		}

		LOGMSGEX( defLOGNAME, defLOG_WORN, "PushControlMesssageQueue full, do release , AfterCount=%u\r\n", ctlMessageList.size() );
	}
}

bool GSIOTClient::CheckControlMesssageQueue( GSIOTDevice *device, DeviceAddress *addr, JID jid, std::string id )
{
	bool blCheck = false;

	gloox::util::MutexGuard mutexguard( m_mutex_ctlMessageList );

	if( ctlMessageList.empty() )
	{
		return false;
	}

	std::list<ControlMessage *>::iterator it = ctlMessageList.begin();
	for(;it!=ctlMessageList.end();it++)
	{
		GSIOTDevice *dev = (*it)->GetDevice();
		if( Compare_Device( dev, device ) 
			&& Compare_GSIOTObjBase( (*it)->GetObj(), addr )
			&& jid == (*it)->GetJid()
			&& id == (*it)->GetId() )
		{
			(*it)->SetNowTime();
			blCheck = true;
			break;
		}
	}

	return blCheck;
}

ControlMessage* GSIOTClient::PopControlMesssageQueue( GSIOTDevice *device, ControlBase *ctl, DeviceAddress *addr, IOTDeviceType specType, IOTDeviceType specExType )
{
	if( !addr && IOT_DEVICE_Unknown==specType && IOT_DEVICE_Unknown==specExType )
	{
		return NULL;
	}

	ControlMessage *pCtlMsg = NULL;

	gloox::util::MutexGuard mutexguard( m_mutex_ctlMessageList );

	if( ctlMessageList.empty() )
	{
		return NULL;
	}

	std::list<ControlMessage *>::iterator it = ctlMessageList.begin();
	for(;it!=ctlMessageList.end();it++)
	{
		GSIOTDevice *dev = (*it)->GetDevice();
		
		// 指定类型获取
		if( !device && !ctl && !addr )
		{
			if( dev->getType()==specType && dev->getExType()==specExType )
			{
				pCtlMsg = (*it);
				ctlMessageList.erase( it );
				break;
			}

			continue;
		}

		if( device )
		{
			if( !Compare_Device( dev, device ) )
			{
				continue;
			}
		}
		else if( ctl )
		{
			if( !Compare_Control( dev->getControl(), ctl ) )
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		if( !Compare_GSIOTObjBase( (*it)->GetObj(), addr ) )
		{
			continue;
		}

		pCtlMsg = (*it);
		ctlMessageList.erase(it);
		break;
	}

	return pCtlMsg;
}

void GSIOTClient::CheckOverTimeControlMesssageQueue()
{
	gloox::util::MutexGuard mutexguard( m_mutex_ctlMessageList );

	CheckOverTimeControlMesssageQueue_nolock();
}

void GSIOTClient::CheckOverTimeControlMesssageQueue_nolock()
{
	ControlMessage *pCtlMsg = NULL;

	if( ctlMessageList.empty() )
	{
		return ;
	}

	std::list<ControlMessage *>::iterator it = ctlMessageList.begin();
	while( it!=ctlMessageList.end() )
	{
		pCtlMsg = (*it);

		if( pCtlMsg->IsOverTime() )
		{
			pCtlMsg->Print( "ctlMessage:: overtime" );

			delete pCtlMsg;
			ctlMessageList.erase(it);
			it = ctlMessageList.begin();
			continue;
		}

		++it;
	}
}

void GSIOTClient::FinalClearControlMesssageQueue()
{
	gloox::util::MutexGuard mutexguard( m_mutex_ctlMessageList );

	if( ctlMessageList.empty() )
	{
		return ;
	}

	std::list<ControlMessage *>::iterator it = ctlMessageList.begin();
	while( it!=ctlMessageList.end() )
	{
		delete (*it);
		++it;
	}
	ctlMessageList.clear();
}

void GSIOTClient::OnDeviceStatusChange()
{
	/*
	if(hasRegistered){
		iotClient->sendDevices(&this->IotDeviceList);
	}*/
}

void GSIOTClient::onConnect()
{
	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient::onConnect" );
}

void GSIOTClient::onDisconnect( ConnectionError e )
{
	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient::onDisconnect(err=%d)", e );

	//printf( "message_test: disconnected: %d\n", e );
	//if( e == ConnAuthenticationFailed )
	//	printf( "auth failed. reason: %d\n", j->authError() );
}

bool GSIOTClient::onTLSConnect( const CertInfo& info )
{
	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient::onTLSConnect" );

	//time_t from( info.date_from );
	//time_t to( info.date_to );

	//printf( "status: %d\nissuer: %s\npeer: %s\nprotocol: %s\nmac: %s\ncipher: %s\ncompression: %s\n"
	//	"from: %s\nto: %s\n",
	//	info.status, info.issuer.c_str(), info.server.c_str(),
	//	info.protocol.c_str(), info.mac.c_str(), info.cipher.c_str(),
	//	info.compression.c_str(), ctime( &from ), ctime( &to ) );
	return true;
}

void GSIOTClient::handleMessage( const Message& msg, MessageSession* session)
{
	std::string subject = msg.subject();
	std::string body = msg.body();
	
	if(body == "help"){
	    
	}
}

void GSIOTClient::handleIqID( const IQ& iq, int context )
{
}

bool GSIOTClient::handleIq( const IQ& iq )
{
	if( iq.from() == this->xmppClient->jid() )
	{
#ifdef _DEBUG
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "handleIq iq.from() == this->jid()!!!" );
#endif
		return true;
	}

	switch( iq.subtype() )
    {
        case IQ::Get:
			{
				// 与服务器的心跳总是通过
				const StanzaExtension *Ping= iq.findExtension(ExtPing);
				if(Ping){
					XmppPrint( iq, "handleIq recv" );

					if(iq.from().full() == XMPP_SERVER_DOMAIN){
						serverPingCount++;
					}
				    return true;
				}

				/*权限控制*/
				GSIOTUser *pUser = m_cfg->m_UserMgr.check_GetUser( iq.from().bare() );

				this->m_cfg->FixOwnerAuth(pUser);

				XmppGSAuth_User *pExXmppGSAuth_User = (XmppGSAuth_User*)iq.findExtension(ExtIotAuthority_User);
				if( pExXmppGSAuth_User )
				{
					handleIq_Get_XmppGSAuth_User( pExXmppGSAuth_User, iq, pUser );
					return true;
				}

#if defined(defTest_defCfgOprt_GetSelf)
				XmppGSAuth *pExXmppGSAuth_Test = (XmppGSAuth*)iq.findExtension(ExtIotAuthority);
				if( pExXmppGSAuth_Test )
				{
					handleIq_Get_XmppGSAuth( pExXmppGSAuth_Test, iq, pUser );
					return true;
				}
#endif

				defGSReturn ret = m_cfg->m_UserMgr.check_User(pUser);
				if( macGSFailed(ret) )
				{
					LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Get: Not found userinfo. no auth.", iq.from().bare().c_str() );

					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSResult( "all", defGSReturn_NoAuth ) );
					XmppClientSend(re,"handleIq Send(all Get ACK)");
					return true;
				}

				XmppGSAuth *pExXmppGSAuth = (XmppGSAuth*)iq.findExtension(ExtIotAuthority);
				if( pExXmppGSAuth )
				{
					handleIq_Get_XmppGSAuth( pExXmppGSAuth, iq, pUser );
					return true;
				}

				XmppGSState *pExXmppGSState = (XmppGSState*)iq.findExtension(ExtIotState);
				if( pExXmppGSState )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_system, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotState ACK)");
						return true;
					}

					std::list<GSIOTDevice *> tempDevGetList;
					std::list<GSIOTDevice *>::const_iterator it = IotDeviceList.begin();
					for(;it!=IotDeviceList.end();it++)
					{
						GSIOTDevice *pTempDev = (*it);

						if( !pTempDev->GetEnable() )
						{
							continue;
						}

						defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pTempDev->getType(), pTempDev->getId() );

						if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
						{
							tempDevGetList.push_back(pTempDev);
						}
					}

					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSState( struTagParam(), 
						deviceClient->GetAllCommunicationState(false),//deviceClient->GetPortState(), 
						IsRUNCODEEnable(defCodeIndex_SYS_GSM) ? ( deviceClient->GetGSM().GetGSMState()==GSMProcess::defGSMState_OK ? 1:0 ) : -1,
						GetAlarmGuardGlobalFlag(),//global GetAllEventsState()
						this->GetAlarmGuardCurState(),
						this->m_IOT_starttime,
						tempDevGetList
						) );
					XmppClientSend(re,"handleIq Send(Get ExtIotState ACK)");
					return true;
				}

				XmppGSChange *pExXmppGSChange = (XmppGSChange*)iq.findExtension(ExtIotChange);
				if( pExXmppGSChange )
				{
					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSChange( struTagParam(), RUNCODE_Get(defCodeIndex_SYS_Change_Global), RUNCODE_Get(defCodeIndex_SYS_Change_Global,defRunCodeValIndex_2) ) );
					XmppClientSend(re,"handleIq Send(Get ExtIotChange ACK)");
					return true;
				}

				XmppGSEvent *pExXmppGSEvent = (XmppGSEvent*)iq.findExtension(ExtIotEvent);
				if( pExXmppGSEvent )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_event, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_EVENT, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotEvent ACK)");
						return true;
					}

					if( !pExXmppGSEvent->GetDevice() )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_EVENT, defGSReturn_Err ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotEvent ACK)");
						return true;
					}

					const std::list<ControlEvent*> &EventsSrc = pExXmppGSEvent->GetEventList();
					std::list<ControlEvent*> EventsDest;

					std::list<ControlEvent *> evtList = m_event->GetEvents();
					std::list<ControlEvent *>::const_iterator it = evtList.begin();
					for(;it!=evtList.end();it++)
					{
						if( (*it)->GetDeviceType() == pExXmppGSEvent->GetDevice()->getType()
							&& (*it)->GetDeviceID() == pExXmppGSEvent->GetDevice()->getId()
							)
						{
							ControlEvent *pClone = (*it)->clone();
							EventsDest.push_back( pClone );

							switch( pClone->GetType() )
							{
							case CONTROL_Event:
								{
									AutoControlEvent *aevt = (AutoControlEvent*)pClone;

									const GSIOTDevice *pCtlDev = this->GetIOTDevice( aevt->GetControlDeviceType(), aevt->GetControlDeviceId() );
									if( pCtlDev )
									{
										aevt->AddEditAttr( "ctrl_devtype_name", pCtlDev->getName() );
										aevt->AddEditAttr( "address_name", GetDeviceAddressName( pCtlDev, aevt->GetAddress() ) );
									}
								}
								break;

							case Eventthing_Event:
								{
									AutoEventthing *aevt = (AutoEventthing*)pClone;

									const GSIOTDevice *pCtlDev = this->GetIOTDevice( aevt->GetControlDeviceType(), aevt->GetControlDeviceId() );
									if( pCtlDev )
									{
										aevt->AddEditAttr( "ctrl_devtype_name", pCtlDev->getName() );
									}
								}
								break;
							}
						}
					}
					
					GSIOTDevice *pDevice = this->GetIOTDevice( pExXmppGSEvent->GetDevice()->getType(), pExXmppGSEvent->GetDevice()->getId() );
					bool AGRunState = false;
					if( pDevice && pDevice->getControl() )
					{
						switch( pDevice->getType() )
						{
						case IOT_DEVICE_Trigger:
							{
								TriggerControl *ctl = (TriggerControl*)pDevice->getControl();
								AGRunState = ctl->GetAGRunState();
							}
							break;

						case IOT_DEVICE_Camera:
							{
								IPCameraBase *ctl = (IPCameraBase*)pDevice->getControl();
								AGRunState = ctl->GetAGRunState();
							}
							break;

						default:
							break;
						}
					}

					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSEvent(pExXmppGSEvent->GetSrcMethod(), pExXmppGSEvent->GetDevice(), EventsDest, AGRunState, struTagParam(), true ) );
					XmppClientSend(re,"handleIq Send(Get ExtIotEvent ACK)");
					return true;
				}

				XmppGSTalk *pExXmppGSTalk = (XmppGSTalk*)iq.findExtension(ExtIotTalk);
				if( pExXmppGSTalk )
				{
					handleIq_Set_XmppGSTalk( pExXmppGSTalk, iq, pUser );
					return true;
				}

				XmppGSPlayback *pExXmppGSPlayback = (XmppGSPlayback*)iq.findExtension(ExtIotPlayback);
				if( pExXmppGSPlayback )
				{
					handleIq_Set_XmppGSPlayback( pExXmppGSPlayback, iq, pUser );
					return true;
				}

				XmppGSRelation *pExXmppGSRelation = (XmppGSRelation*)iq.findExtension(ExtIotRelation);
				if( pExXmppGSRelation )
				{
					handleIq_Get_XmppGSRelation( pExXmppGSRelation, iq, pUser );
					return true;
				}

				XmppGSPreset *pExXmppGSPreset = (XmppGSPreset*)iq.findExtension(ExtIotPreset);
				if( pExXmppGSPreset )
				{
					handleIq_Get_XmppGSPreset( pExXmppGSPreset, iq, pUser );
					return true;
				}

				XmppGSVObj *pExXmppGSVObj = (XmppGSVObj*)iq.findExtension(ExtIotVObj);
				if( pExXmppGSVObj )
				{
					handleIq_Get_XmppGSVObj( pExXmppGSVObj, iq, pUser );
					return true;
				}

				XmppGSTrans *pExXmppGSTrans = (XmppGSTrans*)iq.findExtension( ExtIotTrans );
				if( pExXmppGSTrans )
				{
					handleIq_Get_XmppGSTrans( pExXmppGSTrans, iq, pUser );
					return true;
				}

				XmppGSReport *pExXmppGSReport = (XmppGSReport*)iq.findExtension(ExtIotReport);
				if( pExXmppGSReport )
				{
					handleIq_Get_XmppGSReport( pExXmppGSReport, iq, pUser );
					return true;
				}

				XmppGSUpdate *pExXmppGSUpdate = (XmppGSUpdate*)iq.findExtension(ExtIotUpdate);
				if( pExXmppGSUpdate )
				{
					handleIq_Set_XmppGSUpdate( pExXmppGSUpdate, iq, pUser );
					return true;
				}

				GSIOTInfo *iotInfo = (GSIOTInfo *)iq.findExtension(ExtIot);
				if(iotInfo){
					
					std::list<GSIOTDevice *> tempDevGetList;
					std::list<GSIOTDevice *>::const_iterator it = IotDeviceList.begin();
					for(;it!=IotDeviceList.end();it++)
					{
						GSIOTDevice *pTempDev = (*it);

						if( !iotInfo->isAllType() )
						{
							if( !iotInfo->isInGetType( pTempDev->getType() ) )
							{
								continue;
							}
						}

						if( !pTempDev->GetEnable() )
						{
							continue;
						}

						//bool isValidDevice = true;

						//switch(pTempDev->getType())
						//{
						//case IOT_DEVICE_Camera:
						//	{
						//		IPCameraBase *pTempCam = (IPCameraBase*)pTempDev->getControl();
						//		if( pTempCam )
						//		{
						//			if( !pTempCam->IsConnect() )
						//			{
						//				isValidDevice = false;
						//			}
						//		}
						//	}
						//	break;
						//}

						//if( !isValidDevice )
						//{
						//	continue;
						//}

						defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pTempDev->getType(), pTempDev->getId() );

						if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
						{
							tempDevGetList.push_back(pTempDev);
						}
					}

					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension(new GSIOTInfo(tempDevGetList));
					XmppClientSend(re,"handleIq Send(Get ExtIot ACK)");

					tempDevGetList.clear();
					return true;
				}
				GSIOTDeviceInfo *deviceInfo = (GSIOTDeviceInfo *)iq.findExtension(ExtIotDeviceInfo);
				if(deviceInfo){
					GSIOTDevice *device = deviceInfo->GetDevice();
					if(device){
						std::list<GSIOTDevice *>::const_iterator it = IotDeviceList.begin();
						for(;it!=IotDeviceList.end();it++){
							if((*it)->getId() == device->getId() && (*it)->getType() == device->getType()){

								if( !(*it)->GetEnable() )
								{
									IQ re( IQ::Result, iq.from(), iq.id() );
									re.addExtension( new XmppGSResult( XMLNS_GSIOT_DEVICE, defGSReturn_NoExist ) );
									XmppClientSend( re, "handleIq Send(Get ExtIotDeviceInfo ACK)" );
									return true;
								}

								defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, device->getType(), device->getId() );

								if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
								{
									LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Get ExtIotDeviceInfo: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );

									IQ re( IQ::Result, iq.from(), iq.id());
									re.addExtension( new XmppGSResult( XMLNS_GSIOT_DEVICE, defGSReturn_NoAuth ) );
									XmppClientSend(re,"handleIq Send(Get ExtIotDeviceInfo ACK)");
									return true;
								}

								if( deviceInfo->isShare() )
								{
									const defUserAuth guestAuth = m_cfg->m_UserMgr.check_Auth( m_cfg->m_UserMgr.GetUser(XMPP_GSIOTUser_Guest), device->getType(), device->getId() );
									curAuth = ( defUserAuth_RW==guestAuth ) ? defUserAuth_RW : defUserAuth_RO;
								}
								
								//找到设备,返回设备详细信息
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension(new GSIOTDeviceInfo(*it, curAuth, deviceInfo->isShare()?defRunCodeVal_Spec_Enable:0) );
								XmppClientSend(re,"handleIq Send(Get ExtIotDeviceInfo ACK)");
								return true;
							}
						}
					}
				    return true;
				}
				break;
			}
		case IQ::Set:
			{
				/*权限控制*/
				GSIOTUser *pUser = m_cfg->m_UserMgr.check_GetUser( iq.from().bare() );

				this->m_cfg->FixOwnerAuth(pUser);

				defGSReturn ret = m_cfg->m_UserMgr.check_User(pUser);
				if( macGSFailed(ret) )
				{
					LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: Not found userinfo. no auth.", iq.from().bare().c_str() );

					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSResult( "all", defGSReturn_NoAuth ) );
					XmppClientSend(re,"handleIq Send(all Set ACK)");
					return true;
				}

				// 没有权限时与客户端的会话心跳也不通过
				GSIOTHeartbeat *heartbeat = (GSIOTHeartbeat *)iq.findExtension(ExtIotHeartbeat);
				if(heartbeat){
#if 1
					ipcamClient->UpdateRTMPSession(iq.from(),heartbeat->GetDeviceID());
#else
					// 发现已停止则返回停止状态
					// 老版本客户端未实现处理返回的停止状态，仍发送心跳过来，所以暂时屏蔽返回代码
					if( 0==ipcamClient->UpdateRTMPSession(iq.from(),heartbeat->GetDeviceID()) )
					{
						GSIOTDevice *pCurDev = this->GetIOTDevice(IOT_DEVICE_Camera,heartbeat->GetDeviceID());

						if( pCurDev )
						{
							const defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pCurDev->getType(), pCurDev->getId() );

							IQ re( IQ::Result, iq.from(), iq.id());
							re.addExtension( new GSIOTControl( pCurDev, curAuth ) );
							XmppClientSend(re,"handleIq Send(GSIOTHeartbeat Stop ACK)");
						}
					}
#endif

				    return true;
				}

				XmppGSState *pExXmppGSState = (XmppGSState*)iq.findExtension(ExtIotState);
				if( pExXmppGSState )
				{
					switch( pExXmppGSState->get_cmd() )
					{
					case XmppGSState::defStateCmd_events:
						{
							defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_system, defAuth_ModuleDefaultID );
							if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
							{
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_NoAuth ) );
								XmppClientSend(re,"handleIq Send(Get ExtIotState ACK)");
								return true;
							}

#if 1
							SetAlarmGuardGlobalFlag( pExXmppGSState->get_state_events() ); // this->SetAllEventsState( pExXmppGSState->get_state_events(), "from xmpp", false );
#else
							AutoEventthing aEvt;
							aEvt.SetAllDevice();
							aEvt.SetRunState( pExXmppGSState->get_state_events() );

							DoControlEvent_Eventthing( &aEvt, &aEvt, "Set ExtIotState", false );
#endif

							// ack
							IQ re( IQ::Result, iq.from(), iq.id());
							re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_Success ) );
							XmppClientSend(re,"handleIq Send(Get ExtIotState ACK)");
						}
						break;
						
					case XmppGSState::defStateCmd_alarmguard:
						{
							defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_system, defAuth_ModuleDefaultID );
							if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
							{
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_NoAuth ) );
								XmppClientSend(re,"handleIq Send(Get ExtIotState ACK)");
								return true;
							}

							const std::map<int,XmppGSState::struAGTime> &mapagTimeRef = pExXmppGSState->get_mapagTime();
							for( std::map<int,XmppGSState::struAGTime>::const_iterator it=mapagTimeRef.begin(); it!=mapagTimeRef.end(); ++it )
							{
								const int vecagt_size = it->second.vecagTime.size();
								const int agtime1 = vecagt_size>0 ? it->second.vecagTime[0]:0;
								const int agtime2 = vecagt_size>1 ? it->second.vecagTime[1]:0;
								const int agtime3 = vecagt_size>2 ? it->second.vecagTime[2]:0;

								int allday = it->second.allday;
								//.resetallday
								//if( defAlarmGuardTime_AllDay != allday 
								//	&& defAlarmGuardTime_UnAllDay != allday )
								//{
								//	if( 0==agtime1
								//		&& 0==agtime2
								//		&& 0==agtime3
								//		)
								//	{
								//		allday = defAlarmGuardTime_AllDay;
								//	}
								//}

								this->m_RunCodeMgr.SetCodeAndSaveDb( g_AlarmGuardTimeWNum2Index(it->first), allday, agtime1, agtime2, agtime3, true, true, true, true );
							}

							// ack
							IQ re( IQ::Result, iq.from(), iq.id());
							re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_Success ) );
							XmppClientSend(re,"handleIq Send(Set ExtIotState ACK)");
						}
						break;
						
					case XmppGSState::defStateCmd_exitlearnmod:
						{
							defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_system, defAuth_ModuleDefaultID );
							if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
							{
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_NoAuth ) );
								XmppClientSend(re,"handleIq Send(Set ExtIotState ACK)");
								return true;
							}

							this->deviceClient->SendMOD_set( defMODSysSet_IR_TXCtl_TX, defLinkID_All );
						}
						break;

					case XmppGSState::defStateCmd_reboot:
						{
							defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_reboot, defAuth_ModuleDefaultID );
							if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
							{
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension( new XmppGSResult( XMLNS_GSIOT_STATE, defGSReturn_NoAuth ) );
								XmppClientSend(re,"handleIq Send(Set ExtIotState ACK)");
								return true;
							}

							sys_reset( iq.from().full().c_str(), 1 );
						}
						break;
					}

					return true;
				}

				XmppGSAuth *pExXmppGSAuth = (XmppGSAuth*)iq.findExtension(ExtIotAuthority);
				if( pExXmppGSAuth )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_authority, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_AUTHORITY, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotAuthority ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSAuth->clone() ) );
					return true;
				}

				XmppGSManager *pExXmppGSManager = (XmppGSManager*)iq.findExtension(ExtIotManager);
				if( pExXmppGSManager )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_manager, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_MANAGER, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotManager ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSManager->clone() ) );
					return true;
				}
				
				XmppGSEvent *pExXmppGSEvent = (XmppGSEvent*)iq.findExtension(ExtIotEvent);
				if( pExXmppGSEvent )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_event, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_EVENT, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotEvent ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSEvent->clone() ) );
					return true;
				}

				XmppGSTalk *pExXmppGSTalk = (XmppGSTalk*)iq.findExtension(ExtIotTalk);
				if( pExXmppGSTalk )
				{
					handleIq_Set_XmppGSTalk( pExXmppGSTalk, iq, pUser );
					return true;
				}

				XmppGSRelation *pExXmppGSRelation = (XmppGSRelation*)iq.findExtension(ExtIotRelation);
				if( pExXmppGSRelation )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_manager, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_RELATION, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Set ExtIotRelation ACK)");
						return true;
					}

					curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pExXmppGSRelation->get_device_type(), pExXmppGSRelation->get_device_id() );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_RELATION, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Set ExtIotRelation ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSRelation->clone() ) );
					return true;
				}

				XmppGSPreset *pExXmppGSPreset = (XmppGSPreset*)iq.findExtension(ExtIotPreset);
				if( pExXmppGSPreset )
				{
					if( XmppGSPreset::defPSMethod_goto == pExXmppGSPreset->GetMethod() )
					{
						defGSReturn result = defGSReturn_Success;

						if( IOT_DEVICE_Camera == pExXmppGSPreset->get_device_type() )
						{
							const CPresetObj *pPresetIn = pExXmppGSPreset->GetFristPreset();
							if( pPresetIn )
							{
								const defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pExXmppGSPreset->get_device_type(), pExXmppGSPreset->get_device_id() );

								if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RW ) )
								{
									const GSIOTDevice *device = this->GetIOTDevice( pExXmppGSPreset->get_device_type(), pExXmppGSPreset->get_device_id() );
									if( device )
									{
										IPCameraBase *ctl = (IPCameraBase*)device->getControl();

										if( ctl->GetAdvAttr().get_AdvAttr( defCamAdvAttr_PTZ_Preset ) )
										{
											const CPresetObj *pPresetLocal = ctl->GetPreset(pPresetIn->GetId());
											if( pPresetLocal )
											{
												if( !ctl->SendPTZ( GSPTZ_Goto_Preset, pPresetLocal->GetIndex() ) )
												{
													result = defGSReturn_Err;
												}
											}
											else
											{
												// 对象不存在
												result = defGSReturn_NoExist;
											}
										}
										else
										{
											// 功能不具备
											result = defGSReturn_FunDisable;
										}
									}
									else
									{
										// 对象不存在
										result = defGSReturn_NoExist;
									}
								}
								else
								{
									// 无权限
									result = defGSReturn_NoAuth;
								}
							}
							else
							{
								// 参数错误
								result = defGSReturn_ErrParam;
							}
						}
						else
						{
							// 设备类型不支持
							result = defGSReturn_UnSupport;
						}

						IQ re( IQ::Result, iq.from(), iq.id() );
						re.addExtension( new XmppGSPreset(struTagParam(true,true), pExXmppGSPreset->GetSrcMethod(), pExXmppGSPreset->get_device_type(), pExXmppGSPreset->get_device_id(), defPresetQueue(), result ) );
						XmppClientSend(re,"handleIq Send(Set ExtIotPreset ACK)");
						return true;
					}

					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_manager, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						// ack
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSPreset(struTagParam(true,true), pExXmppGSPreset->GetSrcMethod(), pExXmppGSPreset->get_device_type(), pExXmppGSPreset->get_device_id(), defPresetQueue(), defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Set ExtIotPreset ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSPreset->clone() ) );
					return true;
				}

				XmppGSVObj *pExXmppGSVObj = (XmppGSVObj*)iq.findExtension(ExtIotVObj);
				if( pExXmppGSVObj )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_manager, defAuth_ModuleDefaultID );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						// ack
						IQ re( IQ::Result, iq.from(), iq.id());
						re.addExtension( new XmppGSVObj(struTagParam(true,true), pExXmppGSVObj->GetSrcMethod(), defmapVObjConfig(), defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Set ExtIotVObj ACK)");
						return true;
					}

					this->AddGSMessage( new GSMessage(defGSMsgType_Notify, iq.from(), iq.id(), pExXmppGSVObj->clone() ) );
					return true;
				}

				XmppGSUpdate *pExXmppGSUpdate = (XmppGSUpdate*)iq.findExtension(ExtIotUpdate);
				if( pExXmppGSUpdate )
				{
					handleIq_Set_XmppGSUpdate( pExXmppGSUpdate, iq, pUser );
					return true;
				}

				GSIOTControl *iotControl = (GSIOTControl *)iq.findExtension(ExtIotControl);
				if(iotControl){
					GSIOTDevice *device = iotControl->getDevice();
					if(device){

						GSIOTDevice *pLocalDevice = NULL;
						if( 0 != device->getId() )
						{
							pLocalDevice = this->GetIOTDevice( device->getType(), device->getId() );
							if( !pLocalDevice )
							{
								LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: dev not found, devType=%d, devID=%d", iq.from().bare().c_str(), device->getType(), device->getId() );
								return true;
							}

							if( !pLocalDevice->getControl() )
							{
								LOGMSGEX( defLOGNAME, defLOG_ERROR, "(%s)IQ::Set: dev ctl err, devType=%d, devID=%d", iq.from().bare().c_str(), device->getType(), device->getId() );
								return true;
							}

							if( !pLocalDevice->GetEnable() )
							{
								LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: dev disabled, devType=%d, devID=%d", iq.from().bare().c_str(), device->getType(), device->getId() );
								return true;
							}

							if( pLocalDevice )
							{
								if( device->GetLinkID() != pLocalDevice->GetLinkID() )
								{
									device->SetLinkID( pLocalDevice->GetLinkID() );
								}
							}
						}

						defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, device->getType(), device->getId() );

						if( defUserAuth_Null == curAuth )
						{
							if( iotControl->getNeedRet() )
							{
								IQ re( IQ::Result, iq.from(), iq.id());
								re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
								XmppClientSend(re,"handleIq Send(Set iotControl NoAuth ACK)");
							}

							LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
							return true;
						}

						if(device->getControl()){
							switch(device->getType())
							{
							case IOT_DEVICE_Camera:
								{
									/*视频权限控制*/
									CameraControl *cam_ctl = (CameraControl *)device->getControl();
									const CameraPTZ *ptz = cam_ctl->getPtz();
									const CameraFocal *focal = cam_ctl->getFocal();
									if( cam_ctl->getPTZFlag() && ptz )
									{
										if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RW ) )
										{
											LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set ptz: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
											return true;
										}

										LOGMSG( "cam_ctl ptz devid=%d, cmd=%d, auto=%d, speed=%d\r\n", device->getId(), ptz->getCommand(), ptz->getAutoflag(), ptz->getSpeed() );
										ipcamClient->SendPTZ( device->getId(), (GSPTZ_CtrlCmd)ptz->getCommand(), ptz->getAutoflag(), 0, ptz->getSpeed() );
									}
									else if(  cam_ctl->getFocalFlag() && focal )
									{
										if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RW ) )
										{
											LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set focal: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
											return true;
										}

										LOGMSG( "cam_ctl focal devid=%d, cmd=%d, auto=%d\r\n", device->getId(), focal->GetZoom(), focal->getAutoflag() );
										ipcamClient->SendPTZ( device->getId(), (GSPTZ_CtrlCmd)focal->GetZoom(), focal->getAutoflag(), 0 );
									}
									else if( cam_ctl->HasCmdCtl() )
									{
										if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RW ) )
										{
											LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set track: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
											return true;
										}

										LOGMSG( "cam_ctl track devid=%d, cmd=%d, x=%d, y=%d\r\n", device->getId(), cam_ctl->get_trackCtl(), cam_ctl->get_trackX(), cam_ctl->get_trackY() );

										switch( cam_ctl->get_trackCtl() )
										{
										case GSPTZ_MOTION_TRACK_Enable:
											{
												if( !ipcamClient->SendPTZ( device->getId(), GSPTZ_MOTION_TRACK_Enable, 0 ) )
												{
													IQ re( IQ::Result, iq.from(), iq.id());
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_Err, defNormResultMod_control_MotionTrack ) );
													XmppClientSend(re,"handleIq Send(MOTION_TRACK_Enable failed ACK)");
												}
											}
											break;

										case GSPTZ_MOTION_TRACK_Disable:
											{
												if( !ipcamClient->SendPTZ( device->getId(), GSPTZ_MOTION_TRACK_Disable, 0 ) )
												{
													IQ re( IQ::Result, iq.from(), iq.id());
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_Err, defNormResultMod_control_MotionTrack ) );
													XmppClientSend(re,"handleIq Send(MOTION_TRACK_Disable failed ACK)");
												}
											}
											break;

										case GSPTZ_MANUALTRACE:
										case GSPTZ_MANUALPTZSel:
											{
												int xpos = cam_ctl->get_trackX();
												int ypos = cam_ctl->get_trackY();
												GSPTZ_CtrlCmd ctrlcmd = cam_ctl->get_trackCtl();

												if( GSPTZ_MANUALTRACE == ctrlcmd )
												{
													if( IsRUNCODEEnable(defCodeIndex_SYS_PTZ_TRACE2PTZSel) )
													{
														IPCameraBase *pcam = ipcamClient->GetCamera(device->getId());
														if( pcam && !pcam->isRight_manual_trace() )
														{
															LOGMSG( "cam_ctl track devid=%d, SYS_PTZ_TRACE2PTZSel", device->getId() );

															ctrlcmd = GSPTZ_MANUALPTZSel;
														}
													}
												}

												ipcamClient->SendPTZ( device->getId(), ctrlcmd, xpos*10000 + ypos );
											}
											break;

										case GSPTZ_MANUALZoomRng:
											{
												const int xpos = cam_ctl->get_trackX();
												const int ypos = cam_ctl->get_trackY();
												const int EndXpos = cam_ctl->get_trackEndX();
												const int EndYpos = cam_ctl->get_trackEndY();
												const GSPTZ_CtrlCmd ctrlcmd = cam_ctl->get_trackCtl();

												ipcamClient->SendPTZ( device->getId(), ctrlcmd, xpos*10000 + ypos, EndXpos*10000 + EndYpos );
											}
											break;

										case GSPTZ_PTZ_ParkAction_Enable:
											{
												if( !ipcamClient->SendPTZ( device->getId(), GSPTZ_PTZ_ParkAction_Enable, 0 ) )
												{
													IQ re( IQ::Result, iq.from(), iq.id());
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_Err, defNormResultMod_control_PTZ_ParkAction ) );
													XmppClientSend(re,"handleIq Send(PTZ_ParkAction_Enable failed ACK)");
												}
											}
											break;

										case GSPTZ_PTZ_ParkAction_Disable:
											{
												if( !ipcamClient->SendPTZ( device->getId(), GSPTZ_PTZ_ParkAction_Disable, 0 ) )
												{
													IQ re( IQ::Result, iq.from(), iq.id());
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_Err, defNormResultMod_control_PTZ_ParkAction ) );
													XmppClientSend(re,"handleIq Send(PTZ_ParkAction_Disable failed ACK)");
												}
											}
											break;

										case GSPTZ_DoPrePic:
											{
												defGSReturn ret = defGSReturn_FunDisable;
												if( m_IGSMessageHandler )
												{
													ret = m_IGSMessageHandler->OnControlOperate( defCtrlOprt_DoPrePic, device->getExType(), device, NULL );
												}

												if( macGSFailed( ret ) )
												{
													IQ re( IQ::Result, iq.from(), iq.id() );
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, ret ) );
													XmppClientSend( re, "handleIq Send(DoPrePic failed ACK)" );
												}
											}
											break;
										}
									}
									else if( cam_ctl->isReboot() )
									{
										if( !GSIOTUser::JudgeAuth( m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_reboot, defAuth_ModuleDefaultID ), defUserAuth_WO ) )
										{
											IQ re( IQ::Result, iq.from(), iq.id());
											re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
											XmppClientSend(re,"handleIq Send(Set CamReboot ACK)");
											return true;
										}

										if( !GSIOTUser::JudgeAuth( m_cfg->m_UserMgr.check_Auth( pUser, IOT_DEVICE_Camera, device->getId() ), defUserAuth_WO ) )
										{
											IQ re( IQ::Result, iq.from(), iq.id());
											re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
											XmppClientSend(re,"handleIq Send(Set CamReboot ACK)");
											return true;
										}

										if( ipcamClient->SendPTZ( device->getId(), GSPTZ_CameraReboot, 0 ) )
										{
											LOGMSG( "cam_ctl reboot devid=%d success.\r\n", device->getId() );
										}
										else
										{
											LOGMSG( "cam_ctl reboot devid=%d failed!\r\n", device->getId() );
										}
									}
									else
									{
										if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
										{
											LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set rtmp: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
											return true;
										}

										if(cam_ctl->getProtocol() == "rtmp" || cam_ctl->getProtocol() == "rtmfp"){
											if(cam_ctl->getStatus()=="play"){
#if 0 //--temptest
												time_t curtime = g_GetUTCTime();
												curtime -= curtime%60;
												this->handleIq_Set_XmppGSPlayback( 
													new XmppGSPlayback( 
													struTagParam(), device->getId(), cam_ctl->getUrl(), cam_ctl->getUrl(), 
													XmppGSPlayback::defPBState_Start, curtime-3600, g_GetUTCTime() 
													), iq, m_cfg->m_UserMgr.GetUser( XMPP_GSIOTUser_Admin ) );
												return true;
#endif

												std::string url_use = cam_ctl->getFromUrl();
												std::vector<std::string> url_backup_use = cam_ctl->get_url_backup();

											//#ifdef _DEBUG
											//	if( url_backup_use.empty() )
											//	{
											//		url_backup_use.push_back( url_use+"b1" );
											//		url_backup_use.push_back( url_use+"b2" );
											//	}
											//#endif

												if( IsRUNCODEEnable(defCodeIndex_TEST_StreamServer) )
												{
													const int val2 = RUNCODE_Get(defCodeIndex_TEST_StreamServer,defRunCodeValIndex_2);
													const int val3 = RUNCODE_Get(defCodeIndex_TEST_StreamServer,defRunCodeValIndex_3);

													char ssvr[32] = {0};
													sprintf_s( ssvr, sizeof(ssvr), "%d.%d.%d.%d", val2/1000, val2%1000, val3/1000, val3%1000 );

													g_replace_all_distinct( url_use, "www.gsss.cn", ssvr );

													for( uint32_t iurlbak=0; iurlbak<url_backup_use.size(); ++iurlbak )
													{
														g_replace_all_distinct( url_backup_use[iurlbak], "www.gsss.cn", ssvr );

													#ifdef _DEBUG
														LOGMSG( "urlbak=%s", url_backup_use[iurlbak].c_str() );
													#endif
													}
												}

												//ipcamClient->PushToRTMPServer( iq.from(), device->getId(), url_use );
												if( device->GetEnable() )
												{
													this->PlayMgrCmd_push( defPlayMgrCmd_Start, curAuth, iq, device->getId(), url_use, url_backup_use );
												}
											}else{
												//ipcamClient->StopRTMPSend(iq.from(),device->getId());
												if( device->GetEnable() )
												{
													this->PlayMgrCmd_push( defPlayMgrCmd_Stop, curAuth, iq, device->getId(), "", cam_ctl->get_url_backup()  );
												}
											}
										}
										else
										{
											LOGMSG( "IQ::Set cam play: unsupport %s, from=%s", cam_ctl->getProtocol().c_str(), iq.from().bare().c_str() );
										}
										//GSIOTDevice *dev = device->clone();
										//dev->setControl(ipcamClient->GetCamera(device->getId())->clone());

										//IQ re( IQ::Result, iq.from(), iq.id());
										//re.addExtension( new GSIOTControl( this->GetIOTDevice(IOT_DEVICE_Camera,device->getId()) ) );
										//XmppClientSend(re,"handleIq Send(Set Camera ACK)");
									}
									return true;
								}
							case IOT_DEVICE_RFDevice:
								{
									/*无线权限控制*/
									//RFDeviceControl *ctl = (RFDeviceControl *)device->getControl();
									//if(!this->CheckControlMesssageQueue(device,ctl->GetFristAddress(),iq.from(),iq.id())){
									//    GSIOTDevice *dev = device->clone(false);
									//    RFDeviceControl *msgctl = (RFDeviceControl *)dev->getControl();
									//    PushControlMesssageQueue(new ControlMessage(iq.from(),iq.id(),
									//		dev,msgctl->GetFristAddress()));
									//}
									//this->SendControl(device->getType(), device->getId(), ctl, ctl->GetFristAddress());
									return true;
								}
							case IOT_DEVICE_CANDevice:
								{
									/*总线权限控制*/
									//CANDeviceControl *ctl = (CANDeviceControl *)device->getControl();
									////验证消息是否存在
									//if(!this->CheckControlMesssageQueue(device,ctl->GetFristAddress(),iq.from(),iq.id())){
									//    GSIOTDevice *dev = device->clone(false);
									//    CANDeviceControl *msgctl = (CANDeviceControl *)dev->getControl();
									//    PushControlMesssageQueue(new ControlMessage(iq.from(),iq.id(),
									//		dev,msgctl->GetFristAddress()));
									//}
									//this->SendControl(device->getType(), device->getId(), ctl, ctl->GetFristAddress());
									return true;
								}

							case IOT_DEVICE_RS485:
								{
									/*总线权限控制*/
									RS485DevControl *ctl = (RS485DevControl *)device->getControl();
									//ctl->AddressQueueChangeToOneAddr();

									if( !GSIOTUser::JudgeAuth( curAuth, RS485DevControl::IsReadCmd( ctl->GetCommand() )?defUserAuth_RO:defUserAuth_WO ) )
									{
										if( iotControl->getNeedRet() )
										{
											IQ re( IQ::Result, iq.from(), iq.id());
											re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
											XmppClientSend(re,"handleIq Send(Set RS485DevControl NoAuth ACK)");
										}

										LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set RS485: no auth., curAuth=%d, devType=%d, devID=%d, cmd=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId(), ctl->GetCommand() );

										return true;
									}

									RS485DevControl *pLocalCtl = (RS485DevControl*)pLocalDevice->getControl();

									const defAddressQueue &AddrQue = ctl->GetAddressList();
									defAddressQueue::const_iterator itAddrQue = AddrQue.begin();
									for( ; itAddrQue!=AddrQue.end(); ++itAddrQue )
									{
										DeviceAddress *pCurOneAddr = *itAddrQue; // 一包带多个地址时，一个一个地址分别操作，分别回复

										if( !pCurOneAddr )
											continue;

										DeviceAddress *pLocalAddr = pLocalCtl->GetAddress( pCurOneAddr->GetAddress() );
										if( !pLocalAddr )
										{
											LOGMSG( "(%s)IQ::Set RS485: notfound addr=%d, devType=%d, devID=%d", iq.from().bare().c_str(), pCurOneAddr->GetAddress(), device->getType(), device->getId() );
											continue;
										}
#if 1
									if( RS485DevControl::IsReadCmd( ctl->GetCommand() ) )
									{
										if( pLocalAddr )
										{
											bool isOld = false;
											uint32_t noUpdateTime = 0;

											std::string strCurValue = pLocalAddr->GetCurValue( &isOld, &noUpdateTime );

											if( !isOld )
											{
												device->SetCurValue( pLocalAddr );

												IQ re( IQ::Result, iq.from(), iq.id() );
												re.addExtension( new GSIOTControl( pLocalDevice ) );
												XmppClientSend( re,"handleIq Send(RS485 Read fasttime<<<<<)" );
												return true;
											}
										}
									}
#endif
									GSIOTDevice *sendDev = pLocalDevice->clone(false);
									if( !sendDev )
										continue;

									RS485DevControl *sendCtl = (RS485DevControl*)sendDev->getControl();
									if( sendCtl )
									{
										DeviceAddress *sendAddr = sendCtl->GetAddress(pLocalAddr->GetAddress());
										if( sendAddr )
										{
											uint32_t nextInterval = 1;

											sendCtl->SetCommand( ctl->GetCommand() );

											const bool IsWriteCmd = RS485DevControl::IsWriteCmd( sendCtl->GetCommand() );
											if( IsWriteCmd )
											{
												pLocalDevice->ResetUpdateState( pCurOneAddr->GetAddress() );
												sendAddr->SetCurValue( pCurOneAddr->GetCurValue() );

												// 
												if( pLocalAddr->GetAttrObj().get_AdvAttr(DeviceAddressAttr::defAttr_IsReSwitch)
													|| pLocalAddr->GetAttrObj().get_AdvAttr(DeviceAddressAttr::defAttr_IsAutoBackSwitch)
													)
												{
													const std::string ReSwitchValue = pLocalAddr->GetCurValue()=="1"?"0":"1";
													pLocalAddr->SetCurValue( ReSwitchValue );
													sendAddr->SetCurValue( ReSwitchValue );
												}
											}

											//验证消息是否存在
											if( RS485DevControl::IsReadCmd( sendCtl->GetCommand() )
												&& !this->CheckControlMesssageQueue(sendDev,sendAddr,iq.from(),iq.id()) )
											{
												GSIOTDevice *dev = sendDev->clone(false);
												RS485DevControl *msgctl = (RS485DevControl *)dev->getControl();
												msgctl->AddressQueueChangeToOneAddr( sendAddr->GetAddress() );
												PushControlMesssageQueue( new ControlMessage( iq.from(), iq.id(), dev, msgctl->GetAddress(sendAddr->GetAddress()) ) );
											}

											// 自动回复开关
											if( pLocalAddr->GetAttrObj().get_AdvAttr(DeviceAddressAttr::defAttr_IsAutoBackSwitch) )
											{
												nextInterval = 300;
											}

											this->SendControl( device->getType(), sendDev, sendAddr, defNormSendCtlOvertime, defNormMsgOvertime, nextInterval );

											if( IsWriteCmd )
											{
												if( iotControl->getNeedRet() )
												{
													IQ re( IQ::Result, iq.from(), iq.id());
													re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_SuccExecuted ) );
													XmppClientSend(re,"handleIq Send(Set RS485DevControl executed ACK)");
												}
											}

											// 自动回复开关
											if( pLocalAddr->GetAttrObj().get_AdvAttr(DeviceAddressAttr::defAttr_IsAutoBackSwitch) )
											{
												const bool IsWriteCmd = RS485DevControl::IsWriteCmd( sendCtl->GetCommand() );
												if( IsWriteCmd )
												{
													const std::string AutoBackSwitchValue = sendAddr->GetCurValue()=="1"?"0":"1";
													pLocalAddr->SetCurValue( AutoBackSwitchValue );
													sendAddr->SetCurValue( AutoBackSwitchValue );
													this->SendControl( device->getType(), sendDev, sendAddr );
												}
											}

										}
									}

									macCheckAndDel_Obj(sendDev);
									}
									return true;
								}

							case IOT_DEVICE_Remote:
								{
									const RFRemoteControl *ctl = (RFRemoteControl *)device->getControl();
									const RFRemoteControl *localctl = (RFRemoteControl *)pLocalDevice->getControl();

									if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
									{
										if( iotControl->getNeedRet() )
										{
											IQ re( IQ::Result, iq.from(), iq.id() );
											re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
											XmppClientSend( re, "handleIq Send(Set RFRemoteControl NoAuth ACK)" );
										}

										LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set remote: no auth., curAuth=%d, devType=%d, devID=%d", iq.from().bare().c_str(), curAuth, device->getType(), device->getId() );
										return true;
									}

									switch( localctl->GetExType() )
									{
									case IOTDevice_AC_Ctl:
										{
											const defUserAuth curAuth_acctl = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_acctl, defAuth_ModuleDefaultID );
											if( !GSIOTUser::JudgeAuth( curAuth_acctl, g_IsReadOnlyCmd( ctl->GetCmd() )?defUserAuth_RO:defUserAuth_WO ) )
											{
												IQ re( IQ::Result, iq.from(), iq.id() );
												re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoAuth ) );
												XmppClientSend( re, "handleIq Send(Set AC_Ctl ACK)" );
												return true;
											}
										}
										break;
									default:
										break;
									}

									// 遍历发来的按钮列表
									const defButtonQueue &que = ctl->GetButtonList();
									defButtonQueue::const_iterator it = que.begin();
									defButtonQueue::const_iterator itEnd = que.end();
									for( ; it!=itEnd; ++it )
									{
										RemoteButton *pCurButton = *it;
										const RemoteButton *pLocalButton = localctl->GetButton( pCurButton->GetId() );

										if( pLocalButton )
										{
											if( IsRUNCODEEnable(defCodeIndex_TEST_Develop_NewFunc) )
											{
												const int testid = atoi(pLocalDevice->getVer().c_str());
												if( pLocalDevice->getVer().find("presetDEBUGDEVELOP")!=std::string::npos )
												{
													ipcamClient->SendPTZ( testid, GSPTZ_Goto_Preset, pLocalButton->GetSignalSafe().original[0] );
													continue;
												}
												else
												{
													const bool is_ptzctlDEBUGDEVELOP = ( pLocalDevice->getVer().find("ptzctlDEBUGDEVELOP")!=std::string::npos );
													if( is_ptzctlDEBUGDEVELOP )
													{
														const GSPTZ_CtrlCmd ctlcmd = (GSPTZ_CtrlCmd)pLocalButton->GetSignalSafe().original[0];
														int sleeptime = pLocalButton->GetSignalSafe().original[1];
														int speedlevel = pLocalButton->GetSignalSafe().original[2];

														if( sleeptime < 50 || sleeptime > 5000 )
														{
															sleeptime = 255;
														}

														if( speedlevel < 1 || speedlevel > 100 ) // 7?
														{
															speedlevel = 7;
														}

														if( ctlcmd<120 )
														{
															ipcamClient->SendPTZ( testid, ctlcmd, 0, 0, speedlevel );
															Sleep( sleeptime );
															ipcamClient->SendPTZ( testid, GSPTZ_STOPAll, 0 );
														}

														continue;
													}
												}
											}

											switch( localctl->GetExType() )
											{
											case IOTDevice_AC_Ctl:
											{
												GSIOTDevice *sendDev = pLocalDevice->clone( false );
												RFRemoteControl *sendctl = (RFRemoteControl*)sendDev->getControl();
												sendctl->ButtonQueueChangeToOne( pCurButton->GetId() );
												
												if( defCmd_Null == ctl->GetCmd() )
												{
													sendctl->SetCmd( defCmd_Default );
												}

												PushControlMesssageQueue( new ControlMessage( iq.from(), iq.id(), sendDev, sendctl->GetButton( pCurButton->GetId() ) ) );
												return true;
											}
											break;

											default:
												break;
											}

											GSIOTDevice *sendDev = pLocalDevice->clone( false );
											RFRemoteControl *sendctl = (RFRemoteControl*)sendDev->getControl();
											sendctl->ButtonQueueChangeToOne( pCurButton->GetId() );

											this->SendControl( device->getType(), sendDev, NULL );

											macCheckAndDel_Obj(sendctl);

											if( iotControl->getNeedRet() )
											{
												IQ re( IQ::Result, iq.from(), iq.id() );
												re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_SuccExecuted ) );
												XmppClientSend( re, "handleIq Send(Set RFRemoteControl executed ACK)" );
											}
										}
										else
										{
											if( iotControl->getNeedRet() )
											{
												IQ re( IQ::Result, iq.from(), iq.id() );
												re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, defGSReturn_NoExist ) );
												XmppClientSend( re, "handleIq Send(Set RFRemoteControl NoExist ACK)" );
											}

											LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set remote: button not found, devType=%d, devID=%d, btnid=%d", iq.from().bare().c_str(), device->getType(), device->getId(), pCurButton->GetId() );
										}
									}

									return true;
								}
								break;
							}
						}
					}
					return true;
				}
				break;
			}

		case IQ::Result:
			{
				XmppGSMessage *pExXmppGSMessage = (XmppGSMessage*)iq.findExtension(ExtIotMessage);
				if( pExXmppGSMessage )
				{
					if( defGSReturn_Success == pExXmppGSMessage->get_state() )
					{
						EventNoticeMsg_Remove( pExXmppGSMessage->get_id() );
					}

					return true;
				}
			}
			break;
	}
	return true;
}

void GSIOTClient::handleIq_Get_XmppGSAuth_User( const XmppGSAuth_User *pExXmppGSAuth_User, const IQ& iq, const GSIOTUser *pUser )
{
	defmapGSIOTUser mapUserDest;
	const defmapGSIOTUser &mapUser = m_cfg->m_UserMgr.GetList_User();

	if( defCfgOprt_GetSelf == pExXmppGSAuth_User->GetMethod()
#if defined(defTest_defCfgOprt_GetSelf)
		|| true //--temptest
#endif
		)
	{
		// 不管是否有权限，总是可以用 getself 方法来获取自身概要信息

		const std::string selfJid = iq.from().bare();

		GSIOTUser *pUserRet = NULL;
		const GSIOTUser *pUserSelf = m_cfg->m_UserMgr.GetUser( selfJid );
		if( pUserSelf )
		{
			// 有自身配置
			pUserRet = pUserSelf->clone();
			//pUserRet->SetResult( defGSReturn_Success );
		}
		else
		{
			const GSIOTUser *pUserGuest = m_cfg->m_UserMgr.GetUser( XMPP_GSIOTUser_Guest );
			if( pUserGuest )
			{
				// 没有自身配置则采用来宾配置构建一个用于返回

				pUserRet = pUserGuest->clone();
				//pUserRet->SetResult( defGSReturn_Success );
				//pUserRet->SetJid(selfJid);
				//pUserRet->SetID(0);
				pUserRet->SetName( "来宾" );
			}
			else
			{
				// 自身和来宾都没有配置，则构建一个用于返回信息
				pUserRet = new GSIOTUser();
				//pUserRet->SetResult( defGSReturn_NoExist );
				pUserRet->SetJid(selfJid);
				pUserRet->SetID(0);
				pUserRet->SetName( "(未添加用户)" );
				pUserRet->SetEnable(defDeviceDisable);
			}
		}

		if( pUserRet )
		{
			this->m_cfg->FixOwnerAuth(pUserRet);

			pUserRet->RemoveUnused();
			GSIOTUserMgr::usermapInsert( mapUserDest, pUserRet );
		}
		else
		{
			return;
		}
	}
	else
	{
		const defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_authority, defAuth_ModuleDefaultID );
		if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
		{
			IQ re( IQ::Result, iq.from(), iq.id());
			re.addExtension( new XmppGSResult( XMLNS_GSIOT_AUTHORITY_USER, defGSReturn_NoAuth ) );
			XmppClientSend(re,"handleIq Send(Get ExtIotAuthority_User ACK)");
			return ;
		}

		const std::string keyjid_owner = this->m_cfg->GetOwnerKeyJid();

		const defmapGSIOTUser& needGetUser = pExXmppGSAuth_User->GetList_User();

		for( defmapGSIOTUser::const_iterator it=mapUser.begin(); it!=mapUser.end(); ++it )
		{
			const GSIOTUser *pUser = it->second;
			const std::string keyjid_user = pUser->GetKeyJid();

			if( needGetUser.find( keyjid_user ) != needGetUser.end() )
			{
				GSIOTUser *pUserRet = pUser->clone();

				this->m_cfg->FixOwnerAuth(pUserRet);

				if( defCfgOprt_GetSimple == pExXmppGSAuth_User->GetMethod()
					|| this->m_cfg->isOwnerForKeyJid(keyjid_owner,keyjid_user) )
				{
					pUserRet->RemoveUnused( true );
				}

				GSIOTUserMgr::usermapInsert( mapUserDest, pUserRet );
			}
		}
	}

	IQ re( IQ::Result, iq.from(), iq.id());
	re.addExtension( new XmppGSAuth_User(pExXmppGSAuth_User->GetSrcMethod(), mapUserDest, struTagParam(), true) );
	XmppClientSend(re,"handleIq Send(Get ExtIotAuthority_User ACK)");
}

void GSIOTClient::handleIq_Get_XmppGSAuth( const XmppGSAuth *pExXmppGSAuth, const IQ& iq, const GSIOTUser *pUser )
{
#if !defined(defTest_defCfgOprt_GetSelf)
	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_authority, defAuth_ModuleDefaultID );
	if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		IQ re( IQ::Result, iq.from(), iq.id());
		re.addExtension( new XmppGSResult( XMLNS_GSIOT_AUTHORITY, defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(Get ExtIotAuthority ACK)");
		return ;
	}
#endif

	//const std::string DefaultNoticeJid = m_cfg->GetNoticeJid();
	//if( !DefaultNoticeJid.empty() )
	//{
	//	GSIOTUser *pUser = m_cfg->m_UserMgr.GetUser( DefaultNoticeJid );

	//	if( !pUser->get_UserFlag(defUserFlag_NoticeGroup) )
	//	{
	//		pUser->set_UserFlag( defUserFlag_NoticeGroup, true );
	//	}
	//}

	const defmapGSIOTUser& needGetUser = pExXmppGSAuth->GetList_User();

	defmapGSIOTUser mapUserDest;
	const defmapGSIOTUser &mapUser = m_cfg->m_UserMgr.GetList_User();
	for( defmapGSIOTUser::const_iterator it=mapUser.begin(); it!=mapUser.end(); ++it )
	{
		const GSIOTUser *pUser = it->second;

		if( defUserAuth_RO == curAuth && !pUser->GetEnable() )
		{
			continue;
		}

		GSIOTUser *pUserRet = pUser->clone();
		pUserRet->ResetOnlyAuth(true,false);
		GSIOTUserMgr::usermapInsert( mapUserDest, pUserRet );
	}

	IQ re( IQ::Result, iq.from(), iq.id());
	re.addExtension( new XmppGSAuth(false, pExXmppGSAuth->GetSrcMethod(), mapUserDest, struTagParam(), true ) );
	XmppClientSend(re,"handleIq Send(Get ExtIotAuthority ACK)");
}

void GSIOTClient::handleIq_Set_XmppGSAuth( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotAuthority != pMsg->getpEx()->extensionType() )
		return;

	const XmppGSAuth *pExXmppGSAuth = (const XmppGSAuth*)pMsg->getpEx();

	defmapGSIOTUser mapUserDest;
	GSIOTUserMgr::usermapCopy( mapUserDest, pExXmppGSAuth->GetList_User() );

	for( defmapGSIOTUser::const_iterator it=mapUserDest.begin(); it!=mapUserDest.end(); ++it )
	{
		GSIOTUser *pUser = it->second;
		if( pUser->isMe( pMsg->getFrom().bare() ) )
		{
			pUser->SetResult(defGSReturn_IsSelf);
		}
		else if( this->m_cfg->isOwner(pUser->GetJid()) )
		{
			pUser->SetResult(defGSReturn_ObjEditDisable);
		}
		else
		{
			defGSReturn ret = m_cfg->m_UserMgr.CfgChange_User( pUser, pExXmppGSAuth->GetMethod() );
			//pUser->SetValidAttribute( GSIOTUser::defAttr_all, false );
			pUser->SetResult(ret);
		}
	}

	// ack
	struTagParam TagParam;
	TagParam.isValid = true;
	TagParam.isResult = true;
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSAuth(true, pExXmppGSAuth->GetSrcMethod(), mapUserDest, TagParam, true) );
	XmppClientSend(re,"handleIq Send(Set ExtIotAuthority ACK)");
}

void GSIOTClient::handleIq_Set_XmppGSManager( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotManager != pMsg->getpEx()->extensionType() )
		return;

	const XmppGSManager *pExXmppGSManager = (const XmppGSManager*)pMsg->getpEx();

	GSIOTUser *pUser = m_cfg->m_UserMgr.check_GetUser( pMsg->getFrom().bare() );

	this->m_cfg->FixOwnerAuth(pUser);

	defGSReturn ret = m_cfg->m_UserMgr.check_User(pUser);
	if( macGSFailed(ret) )
	{
		LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: Not found userinfo. no auth.", pMsg->getFrom().bare().c_str() );

		IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
		re.addExtension( new XmppGSResult( "all", defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(all Set ACK)");
		return ;
	}

	defCfgOprt_ method = pExXmppGSManager->GetMethod();

	const std::list<GSIOTDevice*> devices = pExXmppGSManager->GetDeviceList();
	for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
	{
		GSIOTDevice *pDeviceSrc = *it;

		if( defCfgOprt_Add != method )
		{
			defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pDeviceSrc->getType(), pDeviceSrc->getId() );
			if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
			{
				IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_MANAGER, defGSReturn_NoAuth ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotManager ACK)");
				return ;
			}
		}

		switch(method)
		{
		case defCfgOprt_Add:
			{
				add_GSIOTDevice( pDeviceSrc );
			}
			break;

		case defCfgOprt_AddModify:
			break;

		case defCfgOprt_Modify:
			{
				edit_GSIOTDevice( pDeviceSrc );
			}
			break;

		case defCfgOprt_Delete:
			{
				delete_GSIOTDevice( pDeviceSrc );
			}
			break;
		}
	}

	// ack
	struTagParam TagParam;
	TagParam.isValid = true;
	TagParam.isResult = true;
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSManager(pExXmppGSManager->GetSrcMethod(), pExXmppGSManager->GetDeviceList(), TagParam) );
	XmppClientSend(re,"handleIq Send(Set ExtIotManager ACK)");
}

bool GSIOTClient::add_GSIOTDevice( GSIOTDevice *pDeviceSrc )
{
	if( !pDeviceSrc->getControl() )
	{
		pDeviceSrc->SetResult( defGSReturn_Err );
		LOGMSGEX( defLOGNAME, defLOG_INFO, "add_GSIOTDevice: failed, no ctl, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
		return false;
	}

	GSIOTDevice *pLocalDevice = this->GetIOTDevice( pDeviceSrc->getType(), pDeviceSrc->getId() );

	// 判断是否有子配置要处理
	if( !pDeviceSrc->hasChild() )
	{
		// 没有子配置，而设备又已存在
		if( pLocalDevice )
		{
			pDeviceSrc->SetResult( defGSReturn_IsExist );
			LOGMSGEX( defLOGNAME, defLOG_INFO, "add_GSIOTDevice: failed, dev IsExist, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
			return false;
		}
	}

	// edit attr all
	pDeviceSrc->doEditAttrFromAttrMgr_All();

	if( pDeviceSrc->getName().empty() )
	{
		pDeviceSrc->setName( "dev" );
	}

	if( pLocalDevice )
	{
		bool isSuccess = true;

		switch(pLocalDevice->getControl()->GetType())
		{
		case IOT_DEVICE_RS485:
			{
				RS485DevControl *ctl = (RS485DevControl*)pDeviceSrc->getControl();
				RS485DevControl *localctl = (RS485DevControl*)pLocalDevice->getControl();

				// 遍历发来的列表
				const defAddressQueue &AddrQue = ctl->GetAddressList();
				defAddressQueue::const_iterator itAddrQue = AddrQue.begin();
				for( ; itAddrQue!=AddrQue.end(); ++itAddrQue )
				{
					DeviceAddress *pSrcAddr = *itAddrQue;
					pSrcAddr->SetResult( defGSReturn_Null );

					DeviceAddress *pLocalAddr = localctl->GetAddress( pSrcAddr->GetAddress() );
					if( pLocalAddr )
					{
						pSrcAddr->SetResult( defGSReturn_IsExist );
						continue;
					}

					deviceClient->GetDeviceManager()->Add_DeviceAddress( pLocalDevice, pSrcAddr->clone() );
					pSrcAddr->SetResult( defGSReturn_Success );
				}
			}
			break;

		case IOT_DEVICE_Remote:
			{
				RFRemoteControl *ctl = (RFRemoteControl*)pDeviceSrc->getControl();
				RFRemoteControl *localctl = (RFRemoteControl*)pLocalDevice->getControl();

				// 遍历发来的列表
				const defButtonQueue &que = ctl->GetButtonList();
				defButtonQueue::const_iterator it = que.begin();
				defButtonQueue::const_iterator itEnd = que.end();
				for( ; it!=itEnd; ++it )
				{
					RemoteButton *pSrcButton = *it;
					pSrcButton->SetResult( defGSReturn_Null );

					RemoteButton *pLocalButton = localctl->GetButton( pSrcButton->GetId() );
					if( pLocalButton )
					{
						pSrcButton->SetResult( defGSReturn_IsExist );
						isSuccess = false;
						continue;
					}

					deviceClient->GetDeviceManager()->Add_remote_button( pLocalDevice, pSrcButton->clone() );
					pSrcButton->SetResult( defGSReturn_Success );
				}
			}
			break;
		}

		return isSuccess;
	}
	else
	{
		if( IOT_DEVICE_Camera == pDeviceSrc->getType() )
		{
			CameraControl *ctl = (CameraControl*)pDeviceSrc->getControl();

			std::string outAttrValue;
			int module_type = -1;
			if( ctl->FindEditAttr( "module_type", outAttrValue ) )
			{
				module_type = atoi(outAttrValue.c_str());
			}

			IPCameraBase *cam = NULL;
			switch( module_type )
			{
			case CameraType_hik:
				cam = new HikCamera( "", "", "", 0, "", "", "1.0", GSPtzFlag_Null, GSFocalFlag_Null, 0, 0 );
				break;

			case CameraType_dh:
				cam = new DHCamera( "", "", "", 0, "", "", "1.0", GSPtzFlag_Null, GSFocalFlag_Null, 0, 0 );
				break;

			case TI368:
				cam = new TI368Camera( "", "", "", 0, "", "", "1.0", GSPtzFlag_Null, GSFocalFlag_Null, 0, 0 );
				break;

			case SSD1935:
				cam = new SSD1935Camera( "", "", "", 0, "", "", "1.0", GSPtzFlag_Null, GSFocalFlag_Null, 0, 0 );
				break;
			}

			cam->SetName( pDeviceSrc->getName() );
			cam->doEditAttrFromAttrMgr( *ctl );

			this->ipcamClient->AddIPCamera( cam, pDeviceSrc->GetEnable(), &pLocalDevice );
			pDeviceSrc->SetResult( defGSReturn_Success );
		}
		else
		{
			this->deviceClient->AddController( pDeviceSrc->getControl()->clone(), c_DefaultVer, pDeviceSrc->GetEnable(), &pLocalDevice );
			pDeviceSrc->SetResult( defGSReturn_Success );
		}

		if( !pLocalDevice )
		{
			pDeviceSrc->SetResult( defGSReturn_Err );
			LOGMSGEX( defLOGNAME, defLOG_INFO, "add_GSIOTDevice: add new failed, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
			return false;
		}

		return true;
	}

	return false;
}

bool GSIOTClient::edit_GSIOTDevice( GSIOTDevice *pDeviceSrc )
{
	GSIOTDevice *pLocalDevice = this->GetIOTDevice( pDeviceSrc->getType(), pDeviceSrc->getId() );
	if( !pLocalDevice )
	{
		pDeviceSrc->SetResult( defGSReturn_NoExist );
		LOGMSGEX( defLOGNAME, defLOG_INFO, "edit_GSIOTDevice: failed, dev not found, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
		return false;
	}

	if( pDeviceSrc->GetEditAttrMap().empty() )
	{
		pDeviceSrc->SetResult( defGSReturn_Null );
	}
	else
	{
		if( pLocalDevice->doEditAttrFromAttrMgr( *pDeviceSrc ) )
		{
			if( IOT_DEVICE_Camera == pLocalDevice->getType() )
			{
				ipcamClient->ModifyDevice( pLocalDevice );
			}
			else
			{
				deviceClient->ModifyDevice( pLocalDevice );
			}
			pDeviceSrc->SetResult( defGSReturn_Success );
		}
	}

	if( !pLocalDevice->getControl() )
	{
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "edit_GSIOTDevice: dev ctl err, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
		return false;
	}

	if( pDeviceSrc->getControl() )
	{
		switch( pDeviceSrc->getType() )
		{
		case IOT_DEVICE_Camera:
			{
				return edit_CamDevControl( pLocalDevice, pDeviceSrc );
			}

		case IOT_DEVICE_RS485:
			{
				return edit_RS485DevControl( pLocalDevice, pDeviceSrc );
			}

		case IOT_DEVICE_Remote:
			{
				return edit_RFRemoteControl( pLocalDevice, pDeviceSrc );
			}

		default:
			{
				LOGMSGEX( defLOGNAME, defLOG_ERROR, "edit_GSIOTDevice unsupport type=%d", pDeviceSrc->getType() );
				return false;
			}
		}
	}

	return true;
}

bool GSIOTClient::edit_CamDevControl( GSIOTDevice *pLocalDevice, GSIOTDevice *pDeviceSrc )
{
	CameraControl *ctl = (CameraControl*)pDeviceSrc->getControl();
	IPCameraBase *localctl = (IPCameraBase*)pLocalDevice->getControl();
	
	if( localctl->doEditAttrFromAttrMgr( *ctl ) )
	{
		ipcamClient->ModifyDevice( pLocalDevice );
		pDeviceSrc->SetResult( defGSReturn_Success );
		return true;
	}

	return false;
}

bool GSIOTClient::edit_RS485DevControl( GSIOTDevice *pLocalDevice, GSIOTDevice *pDeviceSrc )
{
	RS485DevControl *ctl = (RS485DevControl*)pDeviceSrc->getControl();
	RS485DevControl *localctl = (RS485DevControl*)pLocalDevice->getControl();

	if( localctl->doEditAttrFromAttrMgr( *ctl ) )
	{
		deviceClient->ModifyDevice( pLocalDevice );
		ctl->SetResult( defGSReturn_Success );
	}

	// 遍历发来的列表
	const defAddressQueue &AddrQue = ctl->GetAddressList();
	defAddressQueue::const_iterator itAddrQue = AddrQue.begin();
	for( ; itAddrQue!=AddrQue.end(); ++itAddrQue )
	{
		DeviceAddress *pSrcAddr = *itAddrQue;
		pSrcAddr->SetResult( defGSReturn_Null );

		DeviceAddress *pLocalAddr = localctl->GetAddress( pSrcAddr->GetAddress() );
		if( !pLocalAddr )
		{
			pSrcAddr->SetResult( defGSReturn_NoExist );
			continue;
		}

		if( pLocalAddr->doEditAttrFromAttrMgr( *pSrcAddr ) )
		{
			deviceClient->ModifyAddress( pLocalDevice, pLocalAddr );
			pSrcAddr->SetResult( defGSReturn_Success );
		}
	}

	return true;
}

bool GSIOTClient::edit_RFRemoteControl( GSIOTDevice *pLocalDevice, GSIOTDevice *pDeviceSrc )
{
	RFRemoteControl *ctl = (RFRemoteControl*)pDeviceSrc->getControl();
	RFRemoteControl *localctl = (RFRemoteControl*)pLocalDevice->getControl();

	// 遍历发来的列表
	const defButtonQueue &que = ctl->GetButtonList();
	defButtonQueue::const_iterator it = que.begin();
	defButtonQueue::const_iterator itEnd = que.end();
	for( ; it!=itEnd; ++it )
	{
		RemoteButton *pSrcButton = *it;
		pSrcButton->SetResult( defGSReturn_Null );

		RemoteButton *pLocalButton = localctl->GetButton( pSrcButton->GetId() );
		if( !pLocalButton )
		{
			pSrcButton->SetResult( defGSReturn_NoExist );
			continue;
		}

		if( pLocalButton->doEditAttrFromAttrMgr( *pSrcButton ) )
		{
			deviceClient->GetDeviceManager()->DB_Modify_remote_button( pLocalDevice->getType(), pLocalDevice->getId(), pLocalButton );
			pSrcButton->SetResult( defGSReturn_Success );
		}
	}

	return true;
}

bool GSIOTClient::delete_GSIOTDevice( GSIOTDevice *pDeviceSrc )
{
	GSIOTDevice *pLocalDevice = this->GetIOTDevice( pDeviceSrc->getType(), pDeviceSrc->getId() );

	if( !pLocalDevice )
	{
		pDeviceSrc->SetResult( defGSReturn_NoExist );
		LOGMSGEX( defLOGNAME, defLOG_INFO, "delete_GSIOTDevice: failed, dev NoExist, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
		return false;
	}

	if( pLocalDevice->m_ObjLocker.islock() )
	{
		pDeviceSrc->SetResult( defGSReturn_IsLock );
		LOGMSGEX( defLOGNAME, defLOG_INFO, "delete_GSIOTDevice: failed, dev IsLock, devType=%d, devID=%d", pDeviceSrc->getType(), pDeviceSrc->getId() );
		return false;
	}

	// 判断是否有子配置要处理
	if( !pDeviceSrc->hasChild() )
	{
		if( this->DeleteDevice( pLocalDevice ) )
		{
			pDeviceSrc->SetResult( defGSReturn_Success );
			return true;
		}

		pDeviceSrc->SetResult( defGSReturn_Err );
		return false;
	}

	bool isSuccess = true;

	switch(pLocalDevice->getControl()->GetType())
	{
	case IOT_DEVICE_RS485:
		{
			RS485DevControl *ctl = (RS485DevControl*)pDeviceSrc->getControl();
			RS485DevControl *localctl = (RS485DevControl*)pLocalDevice->getControl();

			// 遍历发来的列表
			const defAddressQueue &AddrQue = ctl->GetAddressList();
			defAddressQueue::const_iterator itAddrQue = AddrQue.begin();
			for( ; itAddrQue!=AddrQue.end(); ++itAddrQue )
			{
				DeviceAddress *pSrcAddr = *itAddrQue;
				pSrcAddr->SetResult( defGSReturn_Null );

				DeviceAddress *pLocalAddr = localctl->GetAddress( pSrcAddr->GetAddress() );
				if( !pLocalAddr )
				{
					pSrcAddr->SetResult( defGSReturn_NoExist );
					continue;
				}

				if( deviceClient->GetDeviceManager()->Delete_DeviceAddress( pLocalDevice, pLocalAddr->GetAddress() ) )
					pSrcAddr->SetResult( defGSReturn_Success );
				else
					pSrcAddr->SetResult( defGSReturn_Err );
			}
		}
		break;

	case IOT_DEVICE_Remote:
		{
			RFRemoteControl *ctl = (RFRemoteControl*)pDeviceSrc->getControl();
			RFRemoteControl *localctl = (RFRemoteControl*)pLocalDevice->getControl();

			// 遍历发来的列表
			const defButtonQueue &que = ctl->GetButtonList();
			defButtonQueue::const_iterator it = que.begin();
			defButtonQueue::const_iterator itEnd = que.end();
			for( ; it!=itEnd; ++it )
			{
				RemoteButton *pSrcButton = *it;
				pSrcButton->SetResult( defGSReturn_Null );

				RemoteButton *pLocalButton = localctl->GetButton( pSrcButton->GetId() );
				if( !pLocalButton )
				{
					pSrcButton->SetResult( defGSReturn_NoExist );
					isSuccess = false;
					continue;
				}

				if( deviceClient->GetDeviceManager()->Delete_remote_button( pLocalDevice, pLocalButton ) )
					pSrcButton->SetResult( defGSReturn_Success );
				else
					pSrcButton->SetResult( defGSReturn_Err );
			}
		}
		break;
	}

	return isSuccess;
}

void GSIOTClient::handleIq_Set_XmppGSEvent( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotEvent != pMsg->getpEx()->extensionType() )
		return;

	const XmppGSEvent *pExXmppGSEvent = (const XmppGSEvent*)pMsg->getpEx();

	GSIOTUser *pUser = m_cfg->m_UserMgr.check_GetUser( pMsg->getFrom().bare() );

	this->m_cfg->FixOwnerAuth(pUser);

	defGSReturn ret = m_cfg->m_UserMgr.check_User(pUser);
	if( macGSFailed(ret) )
	{
		LOGMSGEX( defLOGNAME, defLOG_INFO, "(%s)IQ::Set: Not found userinfo. no auth.", pMsg->getFrom().bare().c_str() );

		IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
		re.addExtension( new XmppGSResult( "all", defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(all Set ACK)");
		return ;
	}

	defCfgOprt_ method = pExXmppGSEvent->GetMethod();
	
	bool needsort = false;

	const std::list<ControlEvent*> &Events = pExXmppGSEvent->GetEventList();
	for( std::list<ControlEvent*>::const_iterator it=Events.begin(); it!=Events.end(); ++it )
	{
		ControlEvent *pSrc = *it;

		defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pSrc->GetDeviceType(), pSrc->GetDeviceID() );
		if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
		{
			IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
			re.addExtension( new XmppGSResult( XMLNS_GSIOT_EVENT, defGSReturn_NoAuth ) );
			XmppClientSend(re,"handleIq Send(Get ExtIotManager ACK)");
			return ;
		}

		switch(method)
		{
		case defCfgOprt_Add:
			{
				add_ControlEvent( pSrc );
				needsort = true;
			}
			break;

		case defCfgOprt_AddModify:
			break;

		case defCfgOprt_Modify:
			{
				edit_ControlEvent( pSrc );
				needsort = true;
			}
			break;

		case defCfgOprt_Delete:
			{
				delete_ControlEvent( pSrc );
			}
			break;
		}
	}

	if( defCfgOprt_Modify == method )
	{
		std::string outAttrValue;
		if( pExXmppGSEvent->FindEditAttr( "state", outAttrValue ) )
		{
			if( pExXmppGSEvent->GetDevice() )
			{
				GSIOTDevice *pDevice = this->GetIOTDevice( pExXmppGSEvent->GetDevice()->getType(), pExXmppGSEvent->GetDevice()->getId() );
				const bool AGRunState = (bool)atoi(outAttrValue.c_str());
				if( pDevice && pDevice->getControl() )
				{
					defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pDevice->getType(), pDevice->getId() );
					if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
					{
						IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
						re.addExtension( new XmppGSResult( XMLNS_GSIOT_EVENT, defGSReturn_NoAuth ) );
						XmppClientSend(re,"handleIq Send(Get ExtIotManager ACK)");
						return ;
					}

					switch( pDevice->getType() )
					{
					case IOT_DEVICE_Trigger:
						{
							TriggerControl *ctl = (TriggerControl*)pDevice->getControl();
							ctl->SetAGRunState( AGRunState );
							this->deviceClient->ModifyDevice( pDevice );
						}
						break;

					case IOT_DEVICE_Camera:
						{
							IPCameraBase *ctl = (IPCameraBase*)pDevice->getControl();
							ctl->SetAGRunState( AGRunState );
							this->ipcamClient->ModifyDevice( pDevice );
						}
						break;

					default:
						break;
					}
				}
			}
		}
	}

	if( needsort )
	{
		m_event->SortEvents();
	}

	// ack
	struTagParam TagParam;
	TagParam.isValid = true;
	TagParam.isResult = true;
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSEvent(pExXmppGSEvent->GetSrcMethod(), pExXmppGSEvent->GetDevice(), (std::list<ControlEvent*> &)Events, 1, TagParam, true) );
	XmppClientSend(re,"handleIq Send(Set ExtIotEvent ACK)");
}

bool GSIOTClient::add_ControlEvent( ControlEvent *pSrc )
{
	if( pSrc->GetType() > Unknown_Event )
	{
		ControlEvent *pSrc_clone = pSrc->clone();
		m_event->AddEvent( pSrc_clone );
		pSrc->SetID( pSrc_clone->GetID() );
		pSrc->SetResult( defGSReturn_Success );
		return true;
	}

	pSrc->SetResult( defGSReturn_Err );
	return false;
}

bool GSIOTClient::edit_ControlEvent( ControlEvent *pSrc )
{
	std::list<ControlEvent *> evtList = m_event->GetEvents();
	std::list<ControlEvent *>::const_iterator it = evtList.begin();
	for(;it!=evtList.end();it++)
	{
		if( (*it)->GetDeviceType() == pSrc->GetDeviceType()
			&& (*it)->GetDeviceID() == pSrc->GetDeviceID()
			&& (*it)->GetType() == pSrc->GetType()
			&& (*it)->GetID() == pSrc->GetID()
			)
		{
			bool doUpdate = false;

			switch(pSrc->GetType())
			{
			case SMS_Event:
			case EMAIL_Event:
			case NOTICE_Event:
				{
					doUpdate = (*it)->doEditAttrFromAttrMgr( *pSrc );
					break;
				}

			case CONTROL_Event:
				{
					doUpdate = (*it)->doEditAttrFromAttrMgr( *pSrc );

					AutoControlEvent *aevtLocal = (AutoControlEvent*)(*it);
					AutoControlEvent *aevtSrc = (AutoControlEvent*)pSrc;
					doUpdate |= aevtLocal->UpdateForOther( aevtSrc );
					break;
				}
				
			case Eventthing_Event:
				{
					doUpdate = (*it)->doEditAttrFromAttrMgr( *pSrc );

					AutoEventthing *aevtLocal = (AutoEventthing*)(*it);
					AutoEventthing *aevtSrc = (AutoEventthing*)pSrc;
					if( aevtSrc->IsAllDevice() )
					{
						doUpdate = true;
						aevtLocal->SetAllDevice();
					}
					else
					{
						if( aevtSrc->GetTempDevice() )
						{
							doUpdate |= aevtLocal->UpdateForDev( aevtSrc->GetTempDevice() );
						}
						else
						{
							pSrc->SetResult( defGSReturn_Err );
							return false;
						}
					}

					aevtLocal->SetRunState( aevtSrc->GetRunState() );

					break;
				}

			default:
				pSrc->SetResult( defGSReturn_Err );
				return false;
			}

			if( doUpdate )
			{
				if( m_event->ModifyEvent( pSrc, NULL ) )
					pSrc->SetResult( defGSReturn_Success );
				else
					pSrc->SetResult( defGSReturn_Err );
			}

			return true;
		}
	}

	pSrc->SetResult( defGSReturn_NoExist );
	return false;
}

bool GSIOTClient::delete_ControlEvent( ControlEvent *pSrc )
{
	std::list<ControlEvent *> evtList = m_event->GetEvents();
	std::list<ControlEvent *>::const_iterator it = evtList.begin();
	for(;it!=evtList.end();it++)
	{
		if( (*it)->GetDeviceType() == pSrc->GetDeviceType()
			&& (*it)->GetDeviceID() == pSrc->GetDeviceID()
			&& (*it)->GetType() == pSrc->GetType()
			&& (*it)->GetID() == pSrc->GetID()
			)
		{
			m_event->DeleteEvent( (*it) );
			pSrc->SetResult( defGSReturn_Success );
			return true;
		}
	}

	pSrc->SetResult( defGSReturn_NoExist );
	return true;
}

void GSIOTClient::handleIq_Set_XmppGSTalk( const XmppGSTalk *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	if( !pXmpp )
	{
		return;
	}

	if( !IsRUNCODEEnable(defCodeIndex_SYS_Talk) )
	{
		LOGMSGEX( defLOGNAME, defLOG_INFO, "XmppGSTalk failed, SYS_Talk Disable!" );

		IQ re( IQ::Result, iq.from(), iq.id());
		re.addExtension( new XmppGSResult( XMLNS_GSIOT_TALK, defGSReturn_FunDisable) );
		XmppClientSend(re,"handleIq Send(Get ExtIotTalk ACK)");
		return;
	}

	// 权限验证
	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_talk, defAuth_ModuleDefaultID );
	if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		IQ re( IQ::Result, iq.from(), iq.id());
		re.addExtension( new XmppGSResult( XMLNS_GSIOT_TALK, defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(Get ExtIotTalk ACK)");
		return ;
	}

	const defvecDevKey &specdev = pXmpp->get_vecdev();
	for( defvecDevKey::const_iterator it=specdev.begin(); it!=specdev.end(); ++it )
	{
		curAuth = m_cfg->m_UserMgr.check_Auth( pUser, it->m_type, it->m_id );
		if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
		{
			IQ re( IQ::Result, iq.from(), iq.id());
			re.addExtension( new XmppGSResult( XMLNS_GSIOT_TALK, defGSReturn_NoAuth ) );
			XmppClientSend(re,"handleIq Send(Get ExtIotTalk ACK)");
			return ;
		}
	}

	switch( pXmpp->get_cmd() )
	{
	case XmppGSTalk::defTalkCmd_request:	// 请求
		{
			// 只判断当前正在播放的是否冲突，在这里不判断上限限制

			const bool isOnlyOneTalk_new = true; // 当前模式为固定一路播放

			unsigned long QueueCount = 0;
			unsigned long PlayCount = 0;
			bool isOnlyOneTalk_cur = false;
			this->m_TalkMgr.GetCountInfo( QueueCount, PlayCount, isOnlyOneTalk_cur );

			const bool isplaying = ( PlayCount>0 );

			bool success = false;
			if( isplaying )
			{
				// 独占模式
				if( isOnlyOneTalk_new || isOnlyOneTalk_cur )
				{
					success = false;
				}
				else // 允许多路
				{
					if( PlayCount >= MAX_TALK )
					{
						success = false;
					}
					else
					{
						success = true;
					}
				}
			}
			else
			{
				success = true;
			}

			IQ re( IQ::Result, iq.from(), iq.id());
			re.addExtension( new XmppGSTalk( struTagParam(), 
				success ? XmppGSTalk::defTalkCmd_accept:XmppGSTalk::defTalkCmd_reject, pXmpp->get_url(), pXmpp->get_vecdev() ) );
			if( success )
				XmppClientSend(re,"handleIq Send(Get ExtIotTalk ACK request success)");
			else
				XmppClientSend(re,"handleIq Send(Get ExtIotTalk ACK request failed)");
		}
		break;

	case XmppGSTalk::defTalkCmd_session:	// 会话
		{
#if 0//#ifdef _DEBUG//test
			defvecDevKey vecdev = pXmpp->get_vecdev();
			if( vecdev.empty() )
			{
				const GSIOTDevice *pDevLast = ipcamClient->GetLastPublishCamDev( iq.from().full() );
				if( pDevLast && pDevLast->GetEnable() && pDevLast->getControl() )
				{
					IPCameraBase *cam_ctl = (IPCameraBase*)pDevLast->getControl();
					if( cam_ctl->isTalkUseCam() ) // 是否使用摄像头对讲
					{
						vecdev.push_back( GSIOTDeviceKey( pDevLast->getType(), pDevLast->getId() ) );
					}
				}
			}
			defGSReturn ret = this->m_TalkMgr.StartTalk( pXmpp->get_url(), iq.from().full(), iq.id(), vecdev, true, true );
#else
			defGSReturn ret = this->m_TalkMgr.StartTalk( pXmpp->get_url(), iq.from().full(), iq.id(), pXmpp->get_vecdev(), true, true );
#endif

			if( macGSFailed(ret) )
			{
				IQ re( IQ::Result, iq.from(), iq.id());
				re.addExtension( new XmppGSTalk( struTagParam(), 
					defGSReturn_IsLock==ret?XmppGSTalk::defTalkCmd_reject:XmppGSTalk::defTalkCmd_quit, pXmpp->get_url(), pXmpp->get_vecdev() ) );
				XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK StartTalk failed)");
			}

			// 异步返回成功
		}
		break;

	case XmppGSTalk::defTalkCmd_adddev:	// 增加对讲设备
		{
			defGSReturn ret = this->m_TalkMgr.AdddevTalk( pXmpp->get_url(), iq.from().full(), iq.id(), pXmpp->get_vecdev() );
			if( macGSFailed(ret) )
			{
				IQ re( IQ::Result, iq.from(), iq.id());
				re.addExtension( new XmppGSTalk( struTagParam(), 
					XmppGSTalk::defTalkCmd_adddev, pXmpp->get_url(), pXmpp->get_vecdev(), false ) );
				XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK AdddevTalk failed)");
			}

			// 异步返回成功
		}
		break;

	case XmppGSTalk::defTalkCmd_removedev:	// 移除对讲设备
		{
			defGSReturn ret = this->m_TalkMgr.RemovedevTalk( pXmpp->get_url(), iq.from().full(), iq.id(), pXmpp->get_vecdev() );
			if( macGSFailed(ret) )
			{
				IQ re( IQ::Result, iq.from(), iq.id());
				re.addExtension( new XmppGSTalk( struTagParam(), 
					XmppGSTalk::defTalkCmd_removedev, pXmpp->get_url(), pXmpp->get_vecdev(), false ) );
				XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK RemovedevTalk failed)");
			}

			// 异步返回成功
		}
		break;

	case XmppGSTalk::defTalkCmd_keepalive:	// 心跳
		{
			bool isOnlyOneTalk = false;
			if( this->m_TalkMgr.isPlaying_url( pXmpp->get_url(), isOnlyOneTalk, true ) )
			{
#if 1
				if( pXmpp->get_vecdev().empty() )
				{
					IQ re( IQ::Result, iq.from(), iq.id());
					re.addExtension( new XmppGSTalk( struTagParam(), 
						XmppGSTalk::defTalkCmd_keepalive, pXmpp->get_url(), pXmpp->get_vecdev() ) );
					XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK keepalive is playing)");
				}
				else
#endif
				{
					this->m_TalkMgr.UrlKey_push_cmd( pXmpp->get_url(), struNewPlay_Param( XmppGSTalk::defTalkCmd_keepalive, pXmpp->get_url(), iq.from().full(), iq.id(), false, pXmpp->get_vecdev() ) );
				}
			}
			else
			{
				IQ re( IQ::Result, iq.from(), iq.id());
				re.addExtension( new XmppGSTalk( struTagParam(), 
					XmppGSTalk::defTalkCmd_quit, pXmpp->get_url(), pXmpp->get_vecdev() ) );
				XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK keepalive is quit)");
			}
		}
		break;

	case XmppGSTalk::defTalkCmd_quit:		// 结束
		{
			bool isOnlyOneTalk = false;
			if( !pXmpp->get_url().empty() 
				&& !this->m_TalkMgr.isPlaying_url(pXmpp->get_url(), isOnlyOneTalk) )
			{
				IQ re( IQ::Result, iq.from(), iq.id());
				re.addExtension( new XmppGSTalk( struTagParam(), 
					XmppGSTalk::defTalkCmd_quit, pXmpp->get_url(), pXmpp->get_vecdev() ) );
				XmppClientSend(re,"handleIq Send(Set ExtIotTalk ACK quitcmd is not playing)");
			}

			this->m_TalkMgr.StopTalk( pXmpp->get_url() );
		}
		break;
		
	case XmppGSTalk::defTalkCmd_forcequit:	// 强制退出
		{
			this->m_TalkMgr.StopTalk_AnyOne();
		}
		break;

	default:
		{
			if( pXmpp->get_strSrcCmd().empty() )
				LOGMSG( "handleIq recv ExtIotTalk failed! cmd is null" );
			else
				LOGMSG( "handleIq recv ExtIotTalk failed! unsupport cmd=\"%s\"", pXmpp->get_strSrcCmd().c_str() );
		}
		break;
	}
}

void GSIOTClient::handleIq_Set_XmppGSPlayback( const XmppGSPlayback *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	if( !pXmpp )
	{
		return;
	}

	// 权限验证
	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_record, defAuth_ModuleDefaultID );
	if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		IQ re( IQ::Result, iq.from(), iq.id());
		re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK)");
		return ;
	}

	PlaybackCmd_push( pXmpp, iq );
}

void GSIOTClient::PlaybackCmd_ProcOneCmd( const struPlaybackCmd *pCmd )
{
	const XmppGSPlayback *pXmpp = pCmd->pXmpp;
	const JID &from_Jid = pCmd->from_Jid;
	const std::string &from_id = pCmd->from_id;

	int speedlevel = 0;

	// check time
	switch( pXmpp->get_state() )
	{
	case XmppGSPlayback::defPBState_StopAll:
		break;

	default:
		{
			if( timeGetTime()-pCmd->timestamp > 20000 )
			{
				LOGMSG( "handleIq recv ExtIotPlayback overtime 10s! jid=%s.", from_Jid.full().c_str() );
				return;
			}
		}
		break;
	}

	// proc
	switch( pXmpp->get_state() )
	{
	case XmppGSPlayback::defPBState_Start:
		{
			// 客户端必须提供url
			if( pXmpp->get_url().empty() && pXmpp->get_url_backup().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! url is null." );
				return;
			}

			// 检查回放通道是否已满
			if( Playback_IsLimit() )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_ResLimit ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! Playback Channel Limit.)");
				return;
			}

#if 1
			// 实例删除
			Playback_DeleteForJid( from_Jid.full() );
#else
			if( Playback_Exist(key) )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_ResLimit ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! Playback key exist.)");
				return;
			}
#endif

			// 时间转换
			struGSTime dtBegin;
			struGSTime dtEnd;
			if( 0==pXmpp->get_startdt() 
				|| 0==pXmpp->get_enddt() 
				|| pXmpp->get_enddt() < pXmpp->get_startdt()
				|| !g_UTCTime_To_struGSTime( pXmpp->get_startdt()-10, dtBegin )
				|| !g_UTCTime_To_struGSTime( pXmpp->get_enddt(), dtEnd ) )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_NoExist ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! param err.)");
				return;
			}

			LOGMSG( "PlaybackCmd_ProcOneCmd camid=%d, begin=%d-%d-%d %d:%d:%d+10?, end=%d-%d-%d %d:%d:%d",
				pXmpp->get_camera_id(),
				dtBegin.Year, dtBegin.Month, dtBegin.Day, dtBegin.Hour, dtBegin.Minute, dtBegin.Second,
				dtEnd.Year, dtEnd.Month, dtEnd.Day, dtEnd.Hour, dtEnd.Minute, dtEnd.Second );

			// 查找回放设备
			const GSIOTDevice *pDev = ipcamClient->GetIOTDevice( pXmpp->get_camera_id() );
			if( !pDev )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_NoExist ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! dev not found.)");
				return;
			}

			if( pDev->getControl() )
			{
				if( defRecMod_NoRec == ((IPCameraBase*)pDev->getControl())->GetRecCfg().getrec_mod() )
				{
					IQ re( IQ::Result, from_Jid, from_id );
					re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_FunDisable ) );
					XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! dev NoRec.)");
					return;
				}
			}

			if( !IsRUNCODEEnable(defCodeIndex_TEST_DeCheckFilePlayback) )
			{
				// 先判断录像是否存在
				const defGSReturn retFind = ((IPCameraBase*)pDev->getControl())->QuickSearchPlayback( &dtBegin, &dtEnd );
				if( macGSFailed(retFind) 
					&& defGSReturn_Null != retFind
					&& defGSReturn_TimeOut != retFind
					)
				{
					IQ re( IQ::Result, from_Jid, from_id );
					re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, retFind ) );
					XmppClientSend( re, "handleIq Send(Get ExtIotPlayback ACK err! QuickSearchPlayback)" );
					return;
				}
			}

			// 克隆设备成回放实例
			GSIOTDevice *PlayCam = GSIOTClient::ClonePlaybackDev( pDev );
			if( !PlayCam )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_Err ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! dev new err.)");
				return;
			}

			IPCameraBase *camctl = ((IPCameraBase*)PlayCam->getControl());
			if( !camctl )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_Err ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! dev ctl new err.)");

				Playback_DeleteDevOne( PlayCam );
				return;
			}
			
			const int sound = pXmpp->get_sound();
			if( sound>=0 )
			{
				LOGMSG( "Playback_Start sound=%d, from=\"%s\"\r\n", sound, from_Jid.full().c_str() );
				camctl->GetStreamObj()->GetRTMPSendObj()->set_playback_sound( sound );
			}
			
			camctl->GetStreamObj()->GetRTMPSendObj()->set_wait_frist_key_frame();
			camctl->GetStreamObj()->OnPublishStart();

			const defGSReturn ret = camctl->Connect( false, NULL, true, &dtBegin, &dtEnd );
			if( macGSFailed(ret) )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, ret ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! dev Connect err.)");

				Playback_DeleteDevOne( PlayCam );
				return;
			}

			// 等待视频基本信息完成
			//DWORD dwStartWait = ::timeGetTime();
			//while( !camctl->GetStreamObj()->HasNAL()
			//	&& ::timeGetTime()-dwStartWait<5000 )
			//{
			//	Sleep(1);
			//}

			std::string useUrl = pXmpp->get_url();

			const bool New_isRTMFP = g_IsRTMFP_url( useUrl );			// 远程发过来的新url是否是RTMFP

			bool doConnect_RTMP = true; // 是否进行RTMP连接
			bool doConnect_RTMFP = false; // 是否进行RTMFP连接

			if( RUNCODE_Get( defCodeIndex_SYS_Enable_RTMFP, defRunCodeValIndex_2 ) )
			{
				if( New_isRTMFP )
				{
					doConnect_RTMFP = true;
					doConnect_RTMP = false;
				}
			}

			if( doConnect_RTMFP )
			{
				camctl->GetStreamObj()->GetRTMPSendObj()->setPeerID( "" );
				camctl->GetStreamObj()->GetRTMPSendObj()->pushRTMPHandle( NULL, useUrl );

				// 后面
				if( IsRUNCODEEnable( defCodeIndex_RTMFP_UrlAddStreamID ) )
				{
					useUrl += "/";
					useUrl += camctl->GetStreamObj()->GetRTMPSendObj()->getStreamID();
				}
			}
			
			// 发布视频
			if( !camctl->GetStreamObj()->SendToRTMPServer( useUrl, true ) )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_Err ) );
				XmppClientSend( re, "handleIq Send(Get ExtIotPlayback ACK err! SendToRTMPServer err.)" );

				Playback_DeleteDevOne( PlayCam );
				return;
			}

			if( doConnect_RTMFP )
			{
				const int waittime = RUNCODE_Get( defCodeIndex_RTMFP_WaitConnTimeout );
				const DWORD dwStart = ::timeGetTime();
				while( camctl->GetStreamObj()->GetRTMPSendObj()->getPeerID().empty() && ::timeGetTime()-dwStart < waittime )
				{
					Sleep( 100 );
				}

				if( camctl->GetStreamObj()->GetRTMPSendObj()->getPeerID().empty() )
				{
					LOGMSG( "CameraBase(%s)::SendToRTMPServer wait rtmfp connect timeout! %dms\r\n", camctl->GetName().c_str(), ::timeGetTime()-dwStart );

					//IQ re( IQ::Result, from_Jid, from_id );
					//re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_Err ) );
					//XmppClientSend( re, "handleIq Send(Get ExtIotPlayback ACK err! wait rtmfp connect timeout!)" );

					//Playback_DeleteDevOne( PlayCam );
					//return;

					doConnect_RTMP = true; // 连接RTMFP失败后自动参试连接RTMP
				}
				else
				{
					camctl->GetStreamObj()->GetRTMPSendObj()->Connect( useUrl );
					LOGMSG( "CameraBase(%s)::SendToRTMPServer wait rtmfp connect success. %dms\r\n", camctl->GetName().c_str(), ::timeGetTime()-dwStart );
				}
			}

			//camctl->MakeKeyFrame();

			if( doConnect_RTMP )
			{
				std::vector<std::string> vecurl;
				if( !New_isRTMFP ) vecurl.push_back( useUrl );
				const std::vector<std::string> &url_backup = pXmpp->get_url_backup();
				for( uint32_t i=0; i<url_backup.size(); ++i )
				{
					vecurl.push_back( url_backup[i] );
				}

				//#ifdef _DEBUG
				//			if( url_backup.empty() )
				//			{
				//				vecurl.push_back( url+"pb1" );
				//				vecurl.push_back( url+"pb2" );
				//			}
				//#endif

				if( IsRUNCODEEnable( defCodeIndex_TEST_StreamServer ) )
				{
					const int val2 = RUNCODE_Get( defCodeIndex_TEST_StreamServer, defRunCodeValIndex_2 );
					const int val3 = RUNCODE_Get( defCodeIndex_TEST_StreamServer, defRunCodeValIndex_3 );

					char ssvr[32] ={0};
					sprintf_s( ssvr, sizeof( ssvr ), "%d.%d.%d.%d", val2/1000, val2%1000, val3/1000, val3%1000 );

					for( uint32_t iurlbak=0; iurlbak<vecurl.size(); ++iurlbak )
					{
						g_replace_all_distinct( vecurl[iurlbak], "www.gsss.cn", ssvr );

#ifdef _DEBUG
						LOGMSG( "pburlbak=%s", vecurl[iurlbak].c_str() );
#endif
					}
				}

				char cntbuf[64] ={0};
				if( IsRUNCODEEnable( defCodeIndex_SYS_UseUrlDifCnt ) )
				{
					static uint32_t s_url_difcnt = timeGetTime();

					for( uint32_t iurlbak=0; iurlbak<vecurl.size(); ++iurlbak )
					{
						vecurl[iurlbak] += std::string( "GS" );
						vecurl[iurlbak] += itoa( timeGetTime() + s_url_difcnt++, cntbuf, 16 );
					}
				}

				defRTMPConnectHandle RTMPhandle = RTMPSend::CreateRTMPInstance( vecurl, useUrl, pDev->getName().c_str() );
				if( !RTMPhandle )
				{
					LOGMSG( "playback(%s)::CreateRTMPInstance Connect rtmp failed\r\n", pDev->getName().c_str() );

					IQ re( IQ::Result, from_Jid, from_id );
					re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_ConnectSvrErr ) );
					XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! ConnectSvrErr.)");

					Playback_DeleteDevOne( PlayCam );
					return;
				}

				camctl->GetStreamObj()->GetRTMPSendObj()->pushRTMPHandle( RTMPhandle, useUrl );
				camctl->GetStreamObj()->GetRTMPSendObj()->Connect( useUrl );
			}


			camctl->setStatus( "playing" );
			//camctl->setUrl( useUrl );
			//camctl->setUrl( camctl->GetStreamObj()->GetRTMPSendObj()->getUrl() );

			const std::string key = useUrl;

			// 新建会话保留指针
			if( !Playback_Add( from_Jid.full(), key, useUrl, camctl->GetStreamObj()->GetRTMPSendObj()->getPeerID(), camctl->GetStreamObj()->GetRTMPSendObj()->getStreamID(), PlayCam ) )
			{
				IQ re( IQ::Result, from_Jid, from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_PLAYBACK, defGSReturn_Err ) );
				XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK err! Playback_Add err.)");

				Playback_DeleteDevOne( PlayCam );
				return;
			}

#if 0 //--temptest
			GSIOTDevice *retdev = PlayCam->clone();
			retdev->setControl( camctl->clone() );
			IQ reTest( IQ::Result, from_Jid, from_id);
			reTest.addExtension(new GSIOTControl(retdev));
			XmppClientSend(reTest,"handleIq Send(Set Playback reTest ACK)");
			return;
#endif

			IQ re( IQ::Result, from_Jid, from_id);
			re.addExtension( new XmppGSPlayback( struTagParam(), 
				pXmpp->get_camera_id(), useUrl, camctl->GetStreamObj()->GetRTMPSendObj()->getPeerID(), camctl->GetStreamObj()->GetRTMPSendObj()->getStreamID(), key, XmppGSPlayback::defPBState_Start // key use url
				) );
			XmppClientSend(re,"handleIq Send(Get ExtIotPlayback ACK)");

			return ;
		}
		break;

	case XmppGSPlayback::defPBState_Stop:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			// 实例删除
			Playback_DeleteForJid( from_Jid.full() );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, GSPlayBackCode_Stop );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_Set:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			const int sound = pXmpp->get_sound();
			if( sound>=0 )
			{
				Playback_SetForJid( from_Jid.full(), sound );
			}

			return ;
		}
		break;

	case XmppGSPlayback::defPBState_Get:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_GetState, 0, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_Pause:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}
			
			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYPAUSE, 0, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_Resume:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYRESTART, 0, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_NormalPlay:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYNORMAL, 0, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_FastPlay:
	case XmppGSPlayback::defPBState_FastPlayThrow:
	case XmppGSPlayback::defPBState_FastPlay1:
	case XmppGSPlayback::defPBState_FastPlay2:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			const int ThrowFrame = XmppGSPlayback::defPBState_FastPlayThrow == pXmpp->get_state() ? 1:0;

			if( XmppGSPlayback::defPBState_FastPlay1 == pXmpp->get_state() || XmppGSPlayback::defPBState_FastPlay2 == pXmpp->get_state() )
			{
				Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYNORMAL );
			}

			// 连续调用两次，用2级播放
			if( XmppGSPlayback::defPBState_FastPlay2 == pXmpp->get_state() )
			{
				Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYFAST, (void*)ThrowFrame );
			}

			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYFAST, (void*)ThrowFrame, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;
		
	case XmppGSPlayback::defPBState_SlowPlay:
	case XmppGSPlayback::defPBState_SlowPlay1:
	case XmppGSPlayback::defPBState_SlowPlay2:
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}

			if( XmppGSPlayback::defPBState_SlowPlay1 == pXmpp->get_state() || XmppGSPlayback::defPBState_SlowPlay2 == pXmpp->get_state() )
			{
				Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYNORMAL );
			}

			// 连续调用两次，用2级播放
			if( XmppGSPlayback::defPBState_SlowPlay2 == pXmpp->get_state() )
			{
				Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYSLOW );
			}

			const GSPlayBackCode_ ControlCode = Playback_CtrlForJid( from_Jid.full(), GSPlayBackCode_PLAYSLOW, 0, 0, &speedlevel );

			Playback_CtrlResult( from_Jid, from_id, pXmpp, ControlCode );
			return ;
		}
		break;

	case XmppGSPlayback::defPBState_StopAll:
		{
			Playback_DeleteAll();

			return ;
		}
		break;

	default: // 心跳
		{
			if( pXmpp->get_key().empty() )
			{
				LOGMSG( "handleIq recv ExtIotPlayback failed! key is null." );
				return;
			}
			
			// 心跳更新
			Playback_UpdateSession( pXmpp->get_key() );

			return ;
		}
		break;
	}
}

void GSIOTClient::PlayMgrCmd_push( defPlayMgrCmd_ cmd, defUserAuth Auth, const IQ& iq, const int dev_id, const std::string &url, const std::vector<std::string> &url_backup )
{
	gloox::util::MutexGuard mutexguard( m_mutex_PlayMgr );

	if( m_lstPlayMgrCmd.size()>999 )
	{
		LOGMSG( "PlayMgrCmd list limit 999! throw jid=%s", iq.from().full().c_str() );
		return ;
	}

	m_lstPlayMgrCmd.push_back( new struPlayMgrCmd( cmd, Auth, iq.from(), iq.id(), dev_id, url, url_backup ) );
}

void GSIOTClient::PlayMgrCmd_SetCheckNow( bool CheckNow )
{
	m_PlayMgr_CheckNowFlag = CheckNow ? defPlayMgrCmd_CheckNow : defPlayMgrCmd_Unknown;
}

bool GSIOTClient::PlayMgrCmd_IsCheckNow()
{
	return ( defPlayMgrCmd_CheckNow == m_PlayMgr_CheckNowFlag );
}

void GSIOTClient::PlayMgrCmd_SetDevtimeNow( IOTDeviceType type, int id )
{
	std::set<int> NeedIDList; // 准备进行校时的队列
	std::list<GSIOTDevice*> &cameraList = this->ipcamClient->GetCameraManager()->GetCameraList();
	std::list<GSIOTDevice*>::const_iterator it = cameraList.begin();
	for( ; it!=cameraList.end(); ++it )
	{
		if( (*it)->GetEnable() )
		{
			if( IOT_DEVICE_All == type )
			{
				NeedIDList.insert( (*it)->getId() );
			}
			else if( (*it)->getType()==type || (*it)->getId()==id )
			{
				NeedIDList.insert( (*it)->getId() );
				break;
			}
		}
	}

	PlayMgrCmd_SetDevtimeNowForList( NeedIDList );
}

void GSIOTClient::PlayMgrCmd_SetDevtimeNowForList( const std::set<int> &NeedIDList )
{
	gloox::util::MutexGuard mutexguard( m_mutex_PlayMgr );

	for( std::set<int>::const_iterator it = NeedIDList.begin(); it!=NeedIDList.end(); ++it )
	{
		m_check_all_devtime_NeedIDList.insert( *it );
	}
}

struPlayMgrCmd* GSIOTClient::PlayMgrCmd_pop()
{
	gloox::util::MutexGuard mutexguard( m_mutex_PlayMgr );

	if( m_lstPlayMgrCmd.empty() )
	{
		return NULL;
	}

	struPlayMgrCmd *pCmd = m_lstPlayMgrCmd.front();
	m_lstPlayMgrCmd.pop_front();
	return pCmd;
}

void GSIOTClient::PlayMgrCmd_clean()
{
	gloox::util::MutexGuard mutexguard( m_mutex_PlayMgr );

	if( m_lstPlayMgrCmd.empty() )
	{
		return;
	}

	for( std::list<struPlayMgrCmd*>::iterator it = m_lstPlayMgrCmd.begin(); it!=m_lstPlayMgrCmd.end(); ++it )
	{
		delete( *it );
	}

	m_lstPlayMgrCmd.clear();
}

// return false : no work
bool GSIOTClient::PlayMgrCmd_OnProc()
{
	struPlayMgrCmd *pCmd = PlayMgrCmd_pop();

	if( !pCmd )
	{
		return false;
	}

	PlayMgrCmd_ProcOneCmd( pCmd );
	delete( pCmd );
	return true;
}

void GSIOTClient::PlayMgrCmd_ProcOneCmd( const struPlayMgrCmd *pCmd )
{
	switch(pCmd->cmd)
	{
	case defPlayMgrCmd_Start:
		{
//#if !defined(_DEBUG)
#if 1
			defGSReturn ret = ipcamClient->PushToRTMPServer( pCmd->from_Jid, pCmd->dev_id, pCmd->url, pCmd->url_backup );
#else
			defGSReturn ret = defGSReturn_Err;
			GSIOTDevice *camdev = this->GetIOTDevice( IOT_DEVICE_Camera, pCmd->dev_id );
			
			if( camdev && camdev->getControl() )
			{
				IPCameraBase *cam = (IPCameraBase*)(camdev)->getControl();
				//if( CameraType_dh==cam->GetCameraType() )
				if( camdev->getVer() == "LINK" )
				{
					if( cam->getUrl().empty() )
					{
						cam->GetStreamObj()->GetRTMPSendObj()->Connect( cam->GetIPAddress() );
						cam->setStatus( "playing" );
					}

					ret = defGSReturn_Success;
				}
				else
				{
					ret = ipcamClient->PushToRTMPServer( pCmd->from_Jid, pCmd->dev_id, pCmd->url, pCmd->url_backup );
				}
			}
			else
			{
				ret = defGSReturn_NoExist;
			}
#endif

			if( macGSFailed(ret) )
			{
				IQ re( IQ::Result, pCmd->from_Jid, pCmd->from_id );
				re.addExtension( new XmppGSResult( XMLNS_GSIOT_CONTROL, ret, defNormResultMod_control_camplay ) );
				XmppClientSend(re,"handleIq Send(PlayMgrCmd_ProcOneCmd Start failed ACK)");
			}
			else
			{
				IQ re( IQ::Result, pCmd->from_Jid, pCmd->from_id );
				re.addExtension( new GSIOTControl( this->GetIOTDevice(IOT_DEVICE_Camera,pCmd->dev_id), pCmd->Auth ) );
				XmppClientSend(re,"handleIq Send(PlayMgrCmd_ProcOneCmd Start ACK)");
			}
		}
		break;

	case defPlayMgrCmd_Stop:
		{
			ipcamClient->StopRTMPSend( pCmd->from_Jid, pCmd->dev_id );

			IQ re( IQ::Result, pCmd->from_Jid, pCmd->from_id );
			re.addExtension( new GSIOTControl( this->GetIOTDevice(IOT_DEVICE_Camera,pCmd->dev_id), pCmd->Auth ) );
			XmppClientSend(re,"handleIq Send(PlayMgrCmd_ProcOneCmd Stop ACK)");
		}
		break;

	default:
		{
			LOGMSGEX( defLOGNAME, defLOG_ERROR, "PlayMgrCmd_ProcOneCmd unknown cmd=%d", pCmd->cmd );
			return;
		}
	}
}

void GSIOTClient::PlayMgrCmd_ThreadCreate()
{
	m_isPlayMgrThreadExit = false;

	LOGMSGEX( defLOGNAME, defLOG_SYS, "PlayMgrProcThread Running..." );
	HANDLE   hth1;
	unsigned  uiThread1ID;
	hth1 = (HANDLE)_beginthreadex( NULL, 0, PlayMgrProcThread, this, 0, &uiThread1ID );
	CloseHandle(hth1);
}

void GSIOTClient::handleIq_Set_XmppGSRelation( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotRelation != pMsg->getpEx()->extensionType() )
		return;

	const XmppGSRelation *pExXmppGSRelation = (const XmppGSRelation*)pMsg->getpEx();

	bool success = m_cfg->SetRelation( pExXmppGSRelation->get_device_type(), pExXmppGSRelation->get_device_id(), pExXmppGSRelation->get_ChildList() );

	// ack
	struTagParam TagParam;
	TagParam.isValid = true;
	TagParam.isResult = true;
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSRelation(TagParam, pExXmppGSRelation->get_device_type(), pExXmppGSRelation->get_device_id(), deflstRelationChild(), success?defGSReturn_Success:defGSReturn_Err ) );
	XmppClientSend(re,"handleIq Send(Set ExtIotRelation ACK)");
}

void GSIOTClient::handleIq_Get_XmppGSRelation( const XmppGSRelation *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pXmpp->get_device_type(), pXmpp->get_device_id() );

	deflstRelationChild ChildList;
	if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		deflstRelationChild tempChildList;
		m_cfg->GetRelation( pXmpp->get_device_type(), pXmpp->get_device_id(), tempChildList );

		for( deflstRelationChild::const_iterator it=tempChildList.begin(); it!=tempChildList.end(); ++it )
		{
			defUserAuth curAuthChild = m_cfg->m_UserMgr.check_Auth( pUser, it->child_dev_type, it->child_dev_id );
			if( GSIOTUser::JudgeAuth( curAuthChild, defUserAuth_RO ) )
			{
				ChildList.push_back( *it );
			}
		}
	}

	IQ re( IQ::Result, iq.from(), iq.id() );
	re.addExtension( new XmppGSRelation(struTagParam(), pXmpp->get_device_type(), pXmpp->get_device_id(), ChildList ) );
	XmppClientSend(re,"handleIq Send(Get ExtIotRelation ACK)");
}

void GSIOTClient::handleIq_Set_XmppGSPreset( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotPreset != pMsg->getpEx()->extensionType() )
		return;

	XmppGSPreset *pXmpp = (XmppGSPreset*)pMsg->getpEx();

	if( !pXmpp )
		return;

	defGSReturn result = defGSReturn_Success;
	defPresetQueue PresetList;

	if( IOT_DEVICE_Camera == pXmpp->get_device_type() )
	{
		const CPresetObj *pPresetIn = pXmpp->GetFristPreset();
		if( pPresetIn )
		{
			GSIOTDevice *device = this->GetIOTDevice( pXmpp->get_device_type(), pXmpp->get_device_id() );
			IPCameraBase *ctl = device?(IPCameraBase*)device->getControl():NULL;

			if( device && ctl )
			{
				if( ctl->GetAdvAttr().get_AdvAttr( defCamAdvAttr_PTZ_Preset ) )
				{
					switch( pXmpp->GetMethod() )
					{
					case XmppGSPreset::defPSMethod_goto:
						{
							// 协议接收处直接执行
						}
						break;

					case XmppGSPreset::defPSMethod_add:
						{
							int newindex = ctl->GetUnusedIndex();
							if( newindex <= 0 )
							{
								// 预置点已满
								result = defGSReturn_ResLimit;
								break;
							}
							
							result = ctl->CheckExist( 0, pPresetIn->GetObjName(), NULL );
							if( macGSFailed(result) )
							{
								break;
							}

							CPresetObj *pPresetNew = new CPresetObj( pPresetIn->GetObjName() );
							pPresetNew->SetIndex( newindex );
							
							if( this->deviceClient->GetDeviceManager()->Add_Preset( device, pPresetNew ) )
							{
								PresetList.push_back( pPresetNew->clone() );
							}
							else
							{
								result = defGSReturn_Err;
								delete pPresetNew;
							}
						}
						break;

					case XmppGSPreset::defPSMethod_del:
						{
							pXmpp->swap_PresetList( PresetList );

							for( defPresetQueue::const_iterator it=PresetList.begin(); it!=PresetList.end(); ++it )
							{
								CPresetObj *pPreset = *it;

								if( !this->deviceClient->GetDeviceManager()->Delete_Preset( device, pPreset->GetId() ) )
								{
									result = defGSReturn_Err;	
								}
							}
						}
						break;

					case XmppGSPreset::defPSMethod_edit:
						{
							pXmpp->swap_PresetList( PresetList );

							for( defPresetQueue::const_iterator it=PresetList.begin(); it!=PresetList.end(); ++it )
							{
								CPresetObj *pPreset = *it;

								result = ctl->CheckExist( pPreset->GetId(), pPreset->GetObjName(), NULL );
								if( macGSFailed(result) )
								{
									break;
								}

								CPresetObj *pPresetLocal = ctl->GetPreset(pPreset->GetId());
								if( pPresetLocal )
								{
									pPresetLocal->SetName( pPreset->GetObjName() );
									if( !this->deviceClient->GetDeviceManager()->DB_Modify_Preset( pPresetLocal, device->getType(), device->getId() ) )
									{
										result = defGSReturn_Err;
										break;
									}
								}
								else
								{
									// 对象不存在
									result = defGSReturn_NoExist;
									break;
								}
							}
						}
						break;

					case XmppGSPreset::defPSMethod_setnew:
						{
							pXmpp->swap_PresetList( PresetList );

							CPresetObj *pPresetLocal = ctl->GetPreset(pPresetIn->GetId());
							if( pPresetLocal )
							{
								if( !ctl->SendPTZ( GSPTZ_SetNew_Preset, pPresetLocal->GetIndex() ) )
								{
									result = defGSReturn_Err;
								}
							}
							else
							{
								// 对象不存在
								result = defGSReturn_NoExist;
							}
						}
						break;

					case XmppGSPreset::defPSMethod_sort:
						{
							pXmpp->swap_PresetList( PresetList );

							ctl->SortByPresetList( PresetList );

							if( !this->deviceClient->GetDeviceManager()->SaveSort_Preset( device ) )
							{
								result = defGSReturn_Err;
							}
						}
						break;

					default:
						result = defGSReturn_UnSupport;
						break;
					}
				}
				else
				{
					// 功能不具备
					result = defGSReturn_FunDisable;
				}
			}
			else
			{
				// 对象不存在
				result = defGSReturn_NoExist;
			}
		}
		else
		{
			// 参数错误
			result = defGSReturn_ErrParam;
		}
	}
	else
	{
		// 设备类型不支持
		result = defGSReturn_UnSupport;
	}

	// ack
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSPreset(struTagParam(true,true), pXmpp->GetSrcMethod(), pXmpp->get_device_type(), pXmpp->get_device_id(), PresetList, result ) );
	XmppClientSend(re,"handleIq Send(Set ExtIotPreset ACK)");
}

void GSIOTClient::handleIq_Get_XmppGSPreset( const XmppGSPreset *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	defGSReturn result = defGSReturn_Null;

	defPresetQueue PresetList;
	if( IOT_DEVICE_Camera == pXmpp->get_device_type() )
	{
		defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pXmpp->get_device_type(), pXmpp->get_device_id() );

		if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
		{
			const GSIOTDevice *device = this->GetIOTDevice( pXmpp->get_device_type(), pXmpp->get_device_id() );
			if( device )
			{
				IPCameraBase *ctl = (IPCameraBase*)device->getControl();

				if( ctl->GetAdvAttr().get_AdvAttr( defCamAdvAttr_PTZ_Preset ) )
				{
					CPresetManager::ClonePresetQueue_Spec( PresetList, ctl->GetPresetList() );
				}
				else
				{
					// 功能不具备
					result = defGSReturn_FunDisable;
				}
			}
			else
			{
				// 对象不存在
				result = defGSReturn_NoExist;
			}
		}
		else
		{
			// 无权限
			result = defGSReturn_NoAuth;
		}
	}
	else
	{
		// 设备类型不支持
		result = defGSReturn_UnSupport;
	}

	IQ re( IQ::Result, iq.from(), iq.id() );
	re.addExtension( new XmppGSPreset(struTagParam(true,true), pXmpp->GetSrcMethod(), pXmpp->get_device_type(), pXmpp->get_device_id(), PresetList, result ) );
	XmppClientSend(re,"handleIq Send(Get ExtIotPreset ACK)");
}

void GSIOTClient::handleIq_Set_XmppGSVObj( const GSMessage *pMsg )
{
	if( !pMsg )
		return;

	if( !pMsg->getpEx() )
		return;

	if( ExtIotVObj != pMsg->getpEx()->extensionType() )
		return;

	XmppGSVObj *pXmpp = (XmppGSVObj*)pMsg->getpEx();

	if( !pXmpp )
		return;

	defGSReturn result = pXmpp->GetResult();
	defmapVObjConfig VObjCfgList = pXmpp->get_VObjCfgList();

	for( defmapVObjConfig::iterator it=VObjCfgList.begin(); it!=VObjCfgList.end(); ++it )
	{
		switch( pXmpp->GetMethod() )
		{
		case defCfgOprt_Add:
			{
				result = m_cfg->VObj_Add( it->second, NULL );
			}
			break;

		case defCfgOprt_Modify:
			{
				result = m_cfg->VObj_Modify( it->second, NULL );
			}
			break;

		case defCfgOprt_Delete:
			{
				result = m_cfg->VObj_Delete( it->second.vobj_type, it->second.id );
			}
			break;
		}
	}

	// ack
	IQ re( IQ::Result, pMsg->getFrom(), pMsg->getId());
	re.addExtension( new XmppGSVObj(struTagParam(true,true), pXmpp->GetSrcMethod(), VObjCfgList, result) );
	XmppClientSend(re,"handleIq Send(Set ExtIotVObj ACK)");
}

void GSIOTClient::handleIq_Get_XmppGSVObj( const XmppGSVObj *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	const defmapVObjConfig &VObjCfgListAll = m_cfg->VObj_GetList();
	defmapVObjConfig VObjCfgListDest;

	for( defmapVObjConfig::const_iterator it=VObjCfgListAll.begin(); it!=VObjCfgListAll.end(); ++it )
	{
		if( !pXmpp->isAllType() )
		{
			if( !pXmpp->isInGetType( it->second.vobj_type ) )
			{
				continue;
			}
		}

		//const defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, it->second.vobj_type, it->second.id );
		//if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
		{
			VObjCfgListDest[it->first] = it->second;
		}
	}

	IQ re( IQ::Result, iq.from(), iq.id() );
	re.addExtension( new XmppGSVObj(struTagParam(true,true), pXmpp->GetSrcMethod(), VObjCfgListDest, defGSReturn_Success ) );
	XmppClientSend(re,"handleIq Send(Get ExtIotVObj ACK)");
}

void GSIOTClient::handleIq_Get_XmppGSTrans( const XmppGSTrans *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	XmppGSTrans *pRetXmpp = new XmppGSTrans( struTagParam( true, true ), pXmpp->get_iXmppType(), pXmpp->get_device_type(), pXmpp->get_device_id(), pXmpp->get_srcPicPreSize(), pXmpp->get_PicPreSize() );
	pRetXmpp->m_result = defGSReturn_Err;

	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pXmpp->get_device_type(), pXmpp->get_device_id() );

	if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		switch( pXmpp->get_iXmppType() )
		{
		case XmppGSTrans::iXmppType_prepic:
		{
			const int PicPreSize = pXmpp->get_PicPreSize();
			if( PicPreSize > defPicPreSize_Unknown && PicPreSize < defPicPreSize_MAX )
			{
				pRetXmpp->m_filetype = "jpg";
				pRetXmpp->m_filename = g_createPicPre_Name(pXmpp->get_device_type(), pXmpp->get_device_id(), str_PicPreSize[PicPreSize]);
				pRetXmpp->m_result = g_FileToBase64( g_createPicPre_FullPathName( pXmpp->get_device_type(), pXmpp->get_device_id(), str_PicPreSize[PicPreSize] ), pRetXmpp->m_base64data ) ? defGSReturn_Success : defGSReturn_Err;
			}
			else
			{
				pRetXmpp->m_result = defGSReturn_ErrParam;
			}

			break;
		}

		case XmppGSTrans::iXmppType_realpic:
		{
			const int PicPreSize = pXmpp->get_PicPreSize();
			if( PicPreSize > defPicPreSize_Unknown && PicPreSize < defPicPreSize_MAX )
			{
				GSIOTDevice *device = this->GetIOTDevice( pXmpp->get_device_type(), pXmpp->get_device_id() );
				if( device )
				{
					char buf[768*1024];
					DWORD bufsize = sizeof( buf );

					defPicPreSize PicPreSize = pXmpp->get_PicPreSize();
					void *inParam = &PicPreSize;

					int *outParam[2];
					outParam[0] = (int*)buf;
					outParam[1] = (int*)&bufsize;

					if( m_IGSMessageHandler )
					{
						pRetXmpp->m_result = m_IGSMessageHandler->OnControlOperate( defCtrlOprt_DoRealPic, device->getExType(), device, NULL, defNormSendCtlOvertime, defNormMsgOvertime, inParam, outParam );
					}
					else
					{
						pRetXmpp->m_result = defGSReturn_UnSupport;
					}

					if( macGSSucceeded(pRetXmpp->m_result) )
					{
						if( g_base64_encode_str( pRetXmpp->m_base64data, (uint8_t*)buf, bufsize ) )
						{
							pRetXmpp->m_result = defGSReturn_Success;
							pRetXmpp->m_filetype = "jpg";
							pRetXmpp->m_filename = g_createPicPre_BaseName( pXmpp->get_device_type(), pXmpp->get_device_id() ) + g_TimeToStr( g_GetUTCTime(), defTimeToStrFmt_UTC ) + ".jpg";
						}
						else
						{
							pRetXmpp->m_result = defGSReturn_Err;
						}
					}
				}
				else
				{
					pRetXmpp->m_result = defGSReturn_NoExist;
				}
			}
			else
			{
				pRetXmpp->m_result = defGSReturn_ErrParam;
			}

			break;
		}

		default:
			pRetXmpp->m_result = defGSReturn_UnSupport;
			break;
		}
	}
	else
	{
		// 无权限
		pRetXmpp->m_result = defGSReturn_NoAuth;
	}
	
	IQ re( IQ::Result, iq.from(), iq.id() );
	re.addExtension( pRetXmpp );
	XmppClientSend( re, "handleIq Send(Get ExtIotTrans ACK)" );
}

void GSIOTClient::handleIq_Get_XmppGSReport( const XmppGSReport *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	XmppGSReport *pRetXmpp = new XmppGSReport(struTagParam(true,true));
	pRetXmpp->CopyParam( *pXmpp );
	pRetXmpp->m_ResultStat.AddrObjKey = pXmpp->m_AddrObjKey;
	pRetXmpp->m_ResultStat.data_dt_begin = g_struGSTime_To_UTCTime( pXmpp->m_dtBegin );
	pRetXmpp->m_ResultStat.data_dt_end = g_struGSTime_To_UTCTime( pXmpp->m_dtEnd );

	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, pXmpp->m_AddrObjKey.dev_type, pXmpp->m_AddrObjKey.dev_id );

	if( GSIOTUser::JudgeAuth( curAuth, defUserAuth_RO ) )
	{
		switch(pXmpp->m_method)
		{
		case XmppGSReport::defRPMethod_minute:
			{
				pRetXmpp->m_result = GetDataStoreMgr()->QuerySrcValueLst_ForTimeRange_QueryStat( pRetXmpp->m_ResultStat, pRetXmpp->m_spanrate, pRetXmpp->m_ratefortype );
			}
			break;

		case XmppGSReport::defRPMethod_hour:
			{
				pRetXmpp->m_result = pRetXmpp->m_getstatminute ?
					GetDataStoreMgr()->QueryStatMinute_ForTime( pRetXmpp->m_ResultStat, pRetXmpp->m_lst_stat, pRetXmpp->m_Interval )
					:
					GetDataStoreMgr()->QueryStatData_ForTime_ForSpanmin( pRetXmpp->m_ResultStat.data_dt_begin, pRetXmpp->m_ResultStat.data_dt_end, pRetXmpp->m_ResultStat, pRetXmpp->m_Interval, pRetXmpp->m_spanrate, pRetXmpp->m_ratefortype );
			}
			break;

		case XmppGSReport::defRPMethod_day:
			{
				pRetXmpp->m_result =
					pRetXmpp->m_getstathour ?
					GetDataStoreMgr()->QueryStatData_ForTime
					(
					pRetXmpp->m_ResultStat.data_dt_begin, pRetXmpp->m_ResultStat.data_dt_end, pRetXmpp->m_ResultStat, pRetXmpp->m_mapRec_stat_day,
					pRetXmpp->m_getstathour, pRetXmpp->m_getdatalist, pRetXmpp->m_Interval, pRetXmpp->m_spanrate, pRetXmpp->m_ratefortype, true
					)
					:
					GetDataStoreMgr()->QueryStatDayRec_ForDayRange
					(
					pRetXmpp->m_ResultStat.data_dt_begin, pRetXmpp->m_ResultStat.data_dt_end, pRetXmpp->m_ResultStat, pRetXmpp->m_mapRec_stat_day,
					true, true, pRetXmpp->m_getdatalist, pRetXmpp->m_Interval, pRetXmpp->m_spanrate, pRetXmpp->m_ratefortype
					);
			}
			break;

		case XmppGSReport::defRPMethod_month:
			{
				pRetXmpp->m_result = GetDataStoreMgr()->QueryStatMonthRec_ForMonthRange
					(
					pRetXmpp->m_ResultStat.data_dt_begin, pRetXmpp->m_ResultStat.data_dt_end, pRetXmpp->m_ResultStat, pRetXmpp->m_mapRec_stat_month, pRetXmpp->m_mapRec_stat_day,
					pRetXmpp->m_getstatday, true, true
					);
			}
			break;
		}
	}
	else
	{
		// 无权限
		pRetXmpp->m_result = defGSReturn_NoAuth;
	}

	if( !pRetXmpp->m_ResultStat.Stat.stat_valid || g_isNoDBRec( pRetXmpp->m_result ) )
	{
		pRetXmpp->m_result = defGSReturn_DBNoRec;
	}

	IQ re( IQ::Result, iq.from(), iq.id() );
	re.addExtension( pRetXmpp );
	XmppClientSend(re,"handleIq Send(Get ExtIotReport ACK)");
}

void GSIOTClient::handleIq_Set_XmppGSUpdate( const XmppGSUpdate *pXmpp, const IQ& iq, const GSIOTUser *pUser )
{
	if( !pXmpp )
	{
		return;
	}

	// 权限验证
	defUserAuth curAuth = m_cfg->m_UserMgr.check_Auth( pUser, IOT_Module_system, defAuth_ModuleDefaultID );
	if( !GSIOTUser::JudgeAuth( curAuth, defUserAuth_WO ) )
	{
		IQ re( IQ::Result, iq.from(), iq.id());
		re.addExtension( new XmppGSResult( XMLNS_GSIOT_UPDATE, defGSReturn_NoAuth ) );
		XmppClientSend(re,"handleIq Send(ExtIotUpdate ACK)");
		return ;
	}

	switch( pXmpp->get_state() )
	{
	case XmppGSUpdate::defUPState_check:
		{
			this->Update_Check_fromremote( iq.from(), iq.id() );
		}
		break;

	case XmppGSUpdate::defUPState_update:
	case XmppGSUpdate::defUPState_forceupdate:
		{
			std::string runparam;
			if( XmppGSUpdate::defUPState_forceupdate == pXmpp->get_state() )
			{
				runparam = "-forceupdate";
			}
			else
			{
				runparam =  "-update";
			}
			
			Update_DoUpdateNow_fromremote( iq.from(), iq.id(), runparam );
		}
		break;

	default:
		LOGMSG( "un support, state=%d", pXmpp->get_state() );
		break;
	}

}

void GSIOTClient::handleSubscription( const Subscription& subscription )
{
	if(subscription.subtype() == Subscription::Subscribe){
		xmppClient->rosterManager()->ackSubscriptionRequest(subscription.from(),true);
	}
}

void GSIOTClient::OnTimer( int TimerID )
{
	if( !m_running )
		return ;

	if( 2 == TimerID )
	{
		// 通知缓存检测
		EventNoticeMsg_Check();
		return ;
	}

	if( 3 == TimerID )
	{
		// 回放会话检测
		Playback_CheckSession();
		Playback_ThreadCheck();
		return ;
	}
	
	if( 4 == TimerID )
	{
		xmppClient->whitespacePing();
		return ;
	}

	if( 5 == TimerID )
	{
		this->CheckSystem();
		this->CheckIOTPs();
		return ;
	}

	if( 1 != TimerID )
		return ;

	char strState_xmpp[256] = {0};
	gloox::ConnectionState state = xmppClient->state();
	switch( state )
	{
	case StateDisconnected:
		sprintf_s( strState_xmpp, sizeof(strState_xmpp), "xmpp curstate(%d) Disconnected", state );
		this->m_xmppReconnect = true;
		break;

	case StateConnecting:
		sprintf_s( strState_xmpp, sizeof(strState_xmpp), "xmpp curstate(%d) Connecting", state );
		break;

	case StateConnected:
		sprintf_s( strState_xmpp, sizeof(strState_xmpp), "xmpp curstate(%d) Connected", state );
		break;

	default:
		sprintf_s( strState_xmpp, sizeof(strState_xmpp), "xmpp curstate(%d)", state );
		break;
	}
	
	LOGMSG( "Heartbeat: %s\r\n", strState_xmpp );

	timeCount++;
	if(timeCount>10){

		LOGMSGEX( defLOGNAME, defLOG_INFO, "GSIOT Version %s (build %s)", g_IOTGetVersion().c_str(), g_IOTGetBuildInfo().c_str() );

		//5分钟内检测一次服务器连接
		xmppClient->whitespacePing();
		if(serverPingCount==0){
			LOGMSGEX( defLOGNAME, defLOG_INFO, "xmppClient serverPingCount=0" );
			//this->m_xmppReconnect = true;
		}
		serverPingCount = 0;
	    timeCount = 0;
	}

	//ipcamClient->CheckRTMPSession();
	deviceClient->Check();

	CheckOverTimeControlMesssageQueue();
}

std::string GSIOTClient::GetConnectStateStr() const
{
	if( !xmppClient )
	{
		return std::string("未注册服务");
	}

	switch( xmppClient->state() )
	{
	case StateDisconnected:
		return std::string("连接中断");

	case StateConnecting:
		return std::string("连接中");

	case StateConnected:
		return std::string("正常");

	default:
		break;
	}

	return std::string("");
}

void GSIOTClient::Run()
{
#if 0
	APlayer ap;
	ap.Open( "rtmp://localhost/gslive/test live=1" );//ap.Open( "d:\\somefile2198a.flv" );
	while( ap.Play() )
	{};
	ap.Close();
#endif

	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient::Run()\r\n" );

	LoadConfig();

	RTMPSend::Init( m_cfg->getSerialNumber() );

	//init devices
	deviceClient = new DeviceConnection(this);
	deviceClient->GetGSM().setGSIOTConfig( m_cfg );
	deviceClient->Run(m_cfg->getSerialPort());

	//init cameras
	ipcamClient = new IPCamConnection(this,this);
	ipcamClient->Connect();

	defDBSavePresetQueue PresetQueue;
	deviceClient->GetDeviceManager()->LoadDB_Preset( PresetQueue );
	ipcamClient->GetCameraManager()->InitAllCamera_Preset( PresetQueue );
}

void GSIOTClient::Connect()
{
	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient::Connect()\r\n" );

	PrintLocalIP();

	std::string strmac = m_cfg->getSerialNumber();
	std::string strjid = m_cfg->getSerialNumber()+"@"+XMPP_SERVER_DOMAIN;

	if(!CheckRegistered()){
		m_cfg->setJid(strjid);
		m_cfg->setPassword(getRandomCode());
		XmppRegister *reg = new XmppRegister(m_cfg->getSerialNumber(),m_cfg->getPassword());
		reg->start();
		bool state = reg->getState();
		delete(reg);
		if(!state){	
			LOGMSGEX( defLOGNAME, defLOG_ERROR, "GSIOTClient::Connect XmppRegister failed!!!" );
		    return;
		}

		m_cfg->SaveToFile();

		SetJidToServer( strjid, strmac );
	}

	/*推送流定时器*/
	timer = new TimerManager();
	timer->registerTimer(this,1,30);
	timer->registerTimer(this,2,2);		// 通知检测
	timer->registerTimer(this,3,15);	// 回放检测
	timer->registerTimer(this,4,60);	// 间隔发ping
	timer->registerTimer(this,5,300);	// check system
	
	JID jid(m_cfg->getJid());
	jid.setResource("gsiot");
	xmppClient = new Client(jid,m_cfg->getPassword());
	//注册物联网协议

	if( RUNCODE_Get( defCodeIndex_SYS_Enable_RTMFP, defRunCodeValIndex_1 )
		|| RUNCODE_Get( defCodeIndex_SYS_Enable_RTMFP, defRunCodeValIndex_2 )
		)
	{
		xmppClient->disco()->addFeature( XMLFUNC_media_rtmfp );
	}

	xmppClient->disco()->addFeature(XMLNS_GSIOT);
	//xmppClient->disco()->addFeature(XMLNS_GSIOT_RESULT);
	//xmppClient->disco()->addFeature(XMLNS_GSIOT_HEARTBEAT);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_CONTROL);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_DEVICE);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_AUTHORITY);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_AUTHORITY_USER);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_MANAGER);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_EVENT);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_STATE);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_Change);

	if( IsRUNCODEEnable(defCodeIndex_SYS_Talk) )
	{
		LOGMSGEX( defLOGNAME, defLOG_SYS, "SYS_Talk=true" );
		xmppClient->disco()->addFeature(XMLNS_GSIOT_TALK);
	}
	else
	{
		LOGMSGEX( defLOGNAME, defLOG_SYS, "SYS_Talk=false" );
	}

	xmppClient->disco()->addFeature(XMLNS_GSIOT_PLAYBACK);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_RELATION);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_Preset);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_VObj);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_Report);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_MESSAGE);
	xmppClient->disco()->addFeature(XMLNS_GSIOT_UPDATE);
	xmppClient->registerStanzaExtension(new GSIOTInfo());
	xmppClient->registerStanzaExtension(new XmppGSResult(NULL));
	xmppClient->registerStanzaExtension(new GSIOTControl());
	xmppClient->registerStanzaExtension(new GSIOTDeviceInfo());
	xmppClient->registerStanzaExtension(new GSIOTHeartbeat());
	xmppClient->registerStanzaExtension(new XmppGSAuth(NULL));
	xmppClient->registerStanzaExtension(new XmppGSAuth_User(NULL));
	xmppClient->registerStanzaExtension(new XmppGSManager(NULL));
	xmppClient->registerStanzaExtension(new XmppGSEvent(NULL));
	xmppClient->registerStanzaExtension(new XmppGSState(NULL));
	xmppClient->registerStanzaExtension(new XmppGSChange(NULL));
	xmppClient->registerStanzaExtension(new XmppGSTalk(NULL));
	xmppClient->registerStanzaExtension(new XmppGSPlayback(NULL));
	xmppClient->registerStanzaExtension(new XmppGSRelation(NULL));
	xmppClient->registerStanzaExtension(new XmppGSPreset(NULL));
	xmppClient->registerStanzaExtension(new XmppGSVObj(NULL));
	xmppClient->registerStanzaExtension(new XmppGSTrans(NULL));
	xmppClient->registerStanzaExtension(new XmppGSReport(NULL));
	xmppClient->registerStanzaExtension(new XmppGSMessage(NULL));
	xmppClient->registerStanzaExtension(new XmppGSUpdate(NULL));
	xmppClient->registerIqHandler(this, ExtIot);
	xmppClient->registerIqHandler(this, ExtIotResult);
	xmppClient->registerIqHandler(this, ExtIotControl);
	xmppClient->registerIqHandler(this, ExtIotDeviceInfo);
	xmppClient->registerIqHandler(this, ExtIotHeartbeat);
	xmppClient->registerIqHandler(this, ExtIotAuthority);
	xmppClient->registerIqHandler(this, ExtIotAuthority_User);
	xmppClient->registerIqHandler(this, ExtIotManager);
	xmppClient->registerIqHandler(this, ExtIotEvent);
	xmppClient->registerIqHandler(this, ExtIotState);
	xmppClient->registerIqHandler(this, ExtIotChange);
	xmppClient->registerIqHandler(this, ExtIotTalk);
	xmppClient->registerIqHandler(this, ExtIotPlayback);
	xmppClient->registerIqHandler(this, ExtIotRelation);
	xmppClient->registerIqHandler(this, ExtIotPreset);
	xmppClient->registerIqHandler(this, ExtIotVObj);
	xmppClient->registerIqHandler(this, ExtIotTrans);
	xmppClient->registerIqHandler(this, ExtIotReport);
	xmppClient->registerIqHandler(this, ExtIotMessage);
	xmppClient->registerIqHandler(this, ExtIotUpdate);
	xmppClient->registerConnectionListener( this );
	//订阅请求直接同意
	xmppClient->registerSubscriptionHandler(this);
	//消息帮助
	xmppClient->registerMessageHandler(this);
	//服务器心跳
	xmppClient->registerIqHandler(this,ExtPing);

#ifdef _DEBUG
	StanzaExtension *test_pStanzaEx = new XmppGSResult(NULL);
	test_pStanzaEx->PrintTag( NULL, NULL ); // 测试PrintTag接口是否被调用，如果未实现，手动在StanzaExtensionFactory中加上PrintTag虚函数
	if( test_pStanzaEx )
	{
		delete test_pStanzaEx;
		test_pStanzaEx = NULL;
	}
#endif


	m_running = true;
	m_isThreadExit = false;

	if( IsRUNCODEEnable(defCodeIndex_Dis_ChangeSaveDB) )
	{
		g_Changed( defCfgOprt_Modify, IOT_Obj_SYS, 0, 0 );
	}

	//
	PlayMgrCmd_ThreadCreate();
	Playback_ThreadCreate();
	DataProc_ThreadCreate();
	AlarmProc_ThreadCreate();
	ACProc_ThreadCreate();

	unsigned long reconnect_tick = timeGetTime();
	LOGMSGEX( defLOGNAME, defLOG_SYS, "GSIOTClient Running...\r\n\r\n" );

	CHeartbeatGuard hbGuard( "XmppClient" );

	m_last_checkNetUseable_time = timeGetTime() - defcheckNetUseable_timeMax + (6*1000);

	while(m_running){
		hbGuard.alive();
#if 1
		ConnectionError ce = ConnNoError;
		if( xmppClient->connect( false ) )
		{
			m_xmppReconnect = false;
			while( ce == ConnNoError && m_running )
			{
				hbGuard.alive();
				if( m_xmppReconnect )
				{
					LOGMSG( "m_xmppReconnect is true, disconnect\n" );
					xmppClient->disconnect();
					break;
				}

				ce = xmppClient->recv(1000);

				this->Update_UpdatedProc();
			}
			LOGMSG( "xmppClient->recv() return %d, m_xmppReconnect=%s\n", ce, m_xmppReconnect?"true":"false" );
		}
#else
	    xmppClient->connect(); // 阻塞式连接
#endif

		uint32_t waittime= RUNCODE_Get(defCodeIndex_xmpp_ConnectInterval);

		if( waittime<6000 )
			waittime=6000;

		const unsigned long prev_span = timeGetTime()-reconnect_tick;
		if( prev_span > waittime*5 )
		{
			waittime=500;
		}
		reconnect_tick = timeGetTime();

		LOGMSGEX( defLOGNAME, defLOG_WORN, ">>>>> xmppClient->connect() return. waittime=%d, prev_span=%lu\r\n", waittime, prev_span );

		DWORD dwStart = ::timeGetTime();
		while( m_running && ::timeGetTime()-dwStart < waittime )
		{
			Sleep(1);
		}
	}
	m_isThreadExit = true;
}

bool GSIOTClient::CheckRegistered()
{
	if(m_cfg->getJid().empty() || m_cfg->getPassword().empty()){
		return false;
	}
	return true;
}

void GSIOTClient::LoadConfig()
{
	if(m_cfg->getSerialNumber()==""){
		std::string macaddress;
		if(getMacAddress(macaddress)==0){
			m_cfg->setSerialNumber(macaddress);
		}
	}
}

bool GSIOTClient::SetJidToServer( const std::string &strjid, const std::string &strmac )
{
	char chreq_setjid[256] = {0};
	sprintf_s( chreq_setjid, sizeof(chreq_setjid), "api.gsss.cn/gsiot.ashx?method=SetJID&jid=%s&mac=%s", strjid.c_str(), strmac.c_str() );
	httpreq::Request req_setjid;
	std::string psHeaderSend;
	std::string psHeaderReceive;
	std::string psMessage;
	if( req_setjid.SendRequest( false, chreq_setjid, psHeaderSend, psHeaderReceive, psMessage ) )
	{
		LOGMSGEX( defLOGNAME, defLOG_INFO, "SetJID to server send success. HeaderReceive=\"%s\", Message=\"%s\"", UTF8ToASCII(psHeaderReceive).c_str(), UTF8ToASCII(psMessage).c_str() );
		return true;
	}

	LOGMSGEX( defLOGNAME, defLOG_ERROR, "SetJID to server send failed." );
	return false;
}

GSIOTDevice* GSIOTClient::CloneCamDev( const GSIOTDevice *src, IPCameraType destType )
{
	if( !src )
		return NULL;

	GSIOTDevice *dev = new GSIOTDevice( *src );

	if( !src->getControl() )
		return dev;

	if( IOT_DEVICE_Camera != src->getType() )
		return dev;

	IPCameraBase *ctl = (IPCameraBase*)src->getControl();
	
	IPCameraType copyType = ctl->GetCameraType();
	if( CameraType_Unkown != destType )
	{
		copyType = destType;
	}

	IPCameraBase *cam = NULL;
	switch( copyType )
	{
	case CameraType_hik:
		cam = new HikCamera( ctl->GetDeviceId(), ctl->GetName(), ctl->GetIPAddress(), ctl->GetPort(), ctl->GetUsername(), ctl->GetPassword(), ctl->getVer(), ctl->getPTZFlag(), ctl->getFocalFlag(), ctl->GetChannel(), ctl->GetStreamfmt() );
		break;

	case CameraType_dh:
		cam = new DHCamera( ctl->GetDeviceId(), ctl->GetName(), ctl->GetIPAddress(), ctl->GetPort(), ctl->GetUsername(), ctl->GetPassword(), ctl->getVer(), ctl->getPTZFlag(), ctl->getFocalFlag(), ctl->GetChannel(), ctl->GetStreamfmt() );
		break;

	case TI368:
		cam = new TI368Camera( ctl->GetDeviceId(), ctl->GetName(), ctl->GetIPAddress(), ctl->GetPort(), ctl->GetUsername(), ctl->GetPassword(), ctl->getVer(), ctl->getPTZFlag(), ctl->getFocalFlag(), ctl->GetChannel(), ctl->GetStreamfmt() );
		break;

	case SSD1935:
		cam = new SSD1935Camera( ctl->GetDeviceId(), ctl->GetName(), ctl->GetIPAddress(), ctl->GetPort(), ctl->GetUsername(), ctl->GetPassword(), ctl->getVer(), ctl->getPTZFlag(), ctl->getFocalFlag(), ctl->GetChannel(), ctl->GetStreamfmt() );
		break;
	}
	cam->UpdateReccfg( ctl->GetRecCfg() );
	cam->UpdateAudioCfg( ctl->GetAudioCfg() );
	cam->UpdateAdvAttr( ctl->GetAdvAttr() );
	dev->setControl( cam );

	return dev;
}

GSIOTDevice* GSIOTClient::ClonePlaybackDev( const GSIOTDevice *src )
{
	if( !src )
	{
		return NULL;
	}

	if( !src->getControl() )
	{
		return NULL;
	}

	IPCameraType destType = CameraType_Unkown;
	if( src->getControl() )
	{
		if( defRecMod_NoRec == ((IPCameraBase*)src->getControl())->GetRecCfg().getrec_mod() )
		{
			return NULL;
		}

		if( defRecMod_OnReordSvr == ((IPCameraBase*)src->getControl())->GetRecCfg().getrec_mod() )
		{
			const IPCameraType curType = ((IPCameraBase*)src->getControl())->GetRecCfg().getrec_svrtype();
			if( CameraType_Unkown == curType
				|| 
				( CameraType_Unkown != curType && CameraType_hik==curType )
				)
			{
				destType = CameraType_hik;
			}
		}
	}

	return GSIOTClient::CloneCamDev( src, destType );
}

ControlBase* GSIOTClient::CloneControl( const ControlBase *src, bool CreateLock )
{
	if( !src )
		return NULL;

	switch( src->GetType() )
	{
	case IOT_DEVICE_RS485:
		{
			return ((RS485DevControl*)src)->clone();
		}
	case IOT_DEVICE_Remote:
		{
			return ((RFRemoteControl*)src)->clone();
		}
	}

	LOGMSGEX( defLOGNAME, defLOG_ERROR, "GSIOTClient::CloneControl Error!!! type=%d\r\n", src->GetType() );
	return NULL;
}

DeviceAddress* GSIOTClient::CloneDeviceAddress( const DeviceAddress *src )
{
	if( src )
	{
		return (DeviceAddress*)(src->clone());
	}

	return NULL;
}

bool GSIOTClient::Compare_Device( const GSIOTDevice *devA, const GSIOTDevice *devB )
{
	if( devA == devB )
		return true;

	if( devA && devB )
	{
		//if( devA->GetLinkID() == devB->GetLinkID() && devA->getId() == devB->getId() && devA->getType() == devB->getType() )
		if( devA->getId() == devB->getId() && devA->getType() == devB->getType() )
		{
			return true;
		}
	}

	return false;
}

bool GSIOTClient::Compare_Control( const ControlBase *ctlA, const ControlBase *ctlB )
{
	if( ctlA == ctlB )
	{
		return true;
	}

	if( ctlA && ctlB )
	{
		if( ctlA->GetLinkID() == ctlB->GetLinkID()  && ctlA->GetExType() == ctlB->GetExType() )
		{
			switch( ctlA->GetExType() )
			{
			case IOT_DEVICE_RFDevice:
				{
					break;
				}
			case IOT_DEVICE_CANDevice:
				{
					break;
				}
			case IOT_DEVICE_RS485:
				{
					if( ((RS485DevControl*)ctlA)->GetDeviceid()==((RS485DevControl*)ctlB)->GetDeviceid() )
					{
						return true;
					}
				}
				break;
			case IOTDevice_AC_Ctl:
				{
					if( ((GSRemoteCtl_AC*)ctlA)->isSameRmtCtl( (GSRemoteCtl_AC*)ctlB ) )
					{
						return true;
					}
				}
				break;
			}
		}
	}

	return false;
}

bool GSIOTClient::Compare_Address( const DeviceAddress *AddrA, const DeviceAddress *AddrB )
{
	if( AddrA == AddrB )
		return true;

	if( AddrA && AddrB )
	{
		if( AddrA->GetAddress() == AddrB->GetAddress() )
		{
			return true;
		}
	}

	return false;
}

bool GSIOTClient::Compare_GSIOTObjBase( const GSIOTObjBase *ObjA, const GSIOTObjBase *ObjB )
{
	if( ObjA == ObjB )
		return true;

	if( ObjA && ObjB )
	{
		if( ObjA->GetObjType()==ObjB->GetObjType() && ObjA->GetId() == ObjB->GetId() )
		{
			return true;
		}
	}

	return false;
}

bool GSIOTClient::Compare_ControlAndAddress( const ControlBase *ctlA, const DeviceAddress *AddrA, const ControlBase *ctlB, const DeviceAddress *AddrB )
{
	if( !GSIOTClient::Compare_Control( ctlA, ctlB ) )
		return false;

	return Compare_Address( AddrA, AddrB );
}

void GSIOTClient::XmppClientSend( const IQ& iq, const char *callinfo )
{
	if( xmppClient )
	{
		XmppPrint( iq, callinfo );
		xmppClient->send( iq );
	}
}

static uint32_t s_XmppClientSend_msg_sno = 0;
void GSIOTClient::XmppClientSend_msg( const JID &to_jid, const std::string &strBody, const std::string &strSubject, const char *callinfo )
{
	if( xmppClient )
	{
		std::string msgid = util::int2string(++s_XmppClientSend_msg_sno);

#if 1
		EventNoticeMsg_Send( to_jid.full(), strSubject, strBody, "XmppClientSend_msg at once" );
#else
		struEventNoticeMsg *msgbuf = new struEventNoticeMsg( msgid, ::timeGetTime(), to_jid, strSubject, strBody, callinfo );
		if( !EventNoticeMsg_Add( msgbuf ) )
		{
			delete msgbuf;
		}
#endif

		Message msg( Message::Normal, to_jid, ASCIIToUTF8(strBody), ASCIIToUTF8(strSubject) );
		msg.setID( msgid );

		XmppPrint( msg, callinfo );
		xmppClient->send( msg );
	}
}

void GSIOTClient::XmppClientSend_jidlist( const std::set<std::string> &jidlist, const std::string &strBody, const std::string &strSubject, const char *callinfo )
{
	LOGMSG( "XmppClientSend_jidlist num=%d\r\n", jidlist.size() );
	
	std::set<std::string>::const_iterator it = jidlist.begin();
	for( ; it!=jidlist.end(); ++it )
	{
		JID to_jid;
		to_jid.setJID( *it );

		if( to_jid )
		{
			XmppClientSend_msg( to_jid, strBody, strSubject, callinfo );
		}
	}
}

void GSIOTClient::XmppPrint( const Message& msg, const char *callinfo )
{
	XmppPrint( msg.tag(), callinfo, NULL );
}

void GSIOTClient::XmppPrint( const IQ& iq, const char *callinfo )
{
	XmppPrint( iq.tag(), callinfo, NULL );
}

void GSIOTClient::XmppPrint( const Tag *ptag, const char *callinfo, const Stanza *stanza, bool dodel )
{
	std::string strxml;
	if( ptag )
	{
		strxml = ptag->xml();
		strxml = UTF8ToASCII( strxml );
	}
	else
	{
		strxml = "<no tag>";
	}
	LOGMSG( "GSIOT %s from=\"%s\", xml=\"%s\"\r\n", callinfo?callinfo:"", stanza?stanza->from().full().c_str():"NULL", strxml.c_str() );
	if( ptag && dodel )
	{
		delete ptag;
	}
}

GSIOTDevice* GSIOTClient::GetIOTDevice( IOTDeviceType deviceType, uint32_t deviceId ) const
{
	if( IOT_DEVICE_Camera == deviceType )
		return ipcamClient->GetIOTDevice( deviceId );

	return deviceClient->GetIOTDevice( deviceType, deviceId );
}

std::string GSIOTClient::GetAddrObjName( const GSIOTAddrObjKey &AddrObjKey ) const
{
	const GSIOTDevice *pDev = this->GetIOTDevice( AddrObjKey.dev_type, AddrObjKey.dev_id );
	if( !pDev )
	{
		return std::string("");
	}

	return pDev->getName() + "-" + GetDeviceAddressName( pDev, AddrObjKey.address_id );
}

std::string GSIOTClient::GetDeviceAddressName( const GSIOTDevice *device, uint32_t address ) const
{
	switch( device->getType() )
	{
	case IOT_DEVICE_Remote:
		{
			RFRemoteControl *ctl = (RFRemoteControl*)device->getControl();
			const RemoteButton *pbtn = ctl->GetButton( address );
			if( pbtn )
			{
				return pbtn->GetObjName();
			}
		}
		break;

	case IOT_DEVICE_RS485:
		{					
			RS485DevControl *ctl = (RS485DevControl *)device->getControl();
			const DeviceAddress *paddr = ctl->GetAddress( address );
			if( paddr )
			{
				return paddr->GetName();
			}
		}
		break;

	case IOT_DEVICE_Camera:
		{					
			IPCameraBase *ctl = (IPCameraBase *)device->getControl();
			const CPresetObj *preset = ctl->GetPreset( address );
			if( preset )
			{
				return preset->GetObjName();
			}
		}
		break;
	}

	return std::string("");
}

bool GSIOTClient::DeleteDevice( GSIOTDevice *iotdevice )
{
	this->m_event->DeleteDeviceEvent( iotdevice->getType(), iotdevice->getId() );

	if( IOT_DEVICE_Camera == iotdevice->getType() )
	{
		return this->ipcamClient->RemoveIPCamera( iotdevice );
	}

	return this->deviceClient->DeleteDevice( iotdevice );
}

bool GSIOTClient::ModifyDevice_Ver( GSIOTDevice *iotdevice, const std::string ver )
{
	if( !iotdevice )
		return false;

	if( !iotdevice->getControl() )
		return false;

	iotdevice->setVer( ver );

	switch( iotdevice->getControl()->GetType() )
	{
	case IOT_DEVICE_RS485:
		{
			RS485DevControl *pCtl = (RS485DevControl*)iotdevice->getControl();
			pCtl->setVer( ver );
		}
		break;
	}

	if( IOT_DEVICE_Camera == iotdevice->getType() )
	{
		ipcamClient->ModifyDevice( iotdevice );
	}
	else
	{
		deviceClient->ModifyDevice( iotdevice );
	}

	return true;
}

void GSIOTClient::PlaybackCmd_DeleteCmd( struPlaybackCmd *pCmd )
{
	if( pCmd )
	{
		if( pCmd->pXmpp )
		{
			delete pCmd->pXmpp;
			pCmd->pXmpp = NULL;
		}

		delete pCmd;
	}
}

void GSIOTClient::PlaybackCmd_push( const XmppGSPlayback *pXmpp, const IQ& iq )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackCmd.size()>999 )
	{
		LOGMSG( "PlaybackCmd list limit 999! throw jid=%s", iq.from().full().c_str() );
		return ;
	}

	m_lstPlaybackCmd.push_back( new struPlaybackCmd( iq.from(), iq.id(), pXmpp ) );
}

struPlaybackCmd* GSIOTClient::PlaybackCmd_pop()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	m_PlaybackThreadTick = timeGetTime();

	if( m_lstPlaybackCmd.empty() )
	{
		return NULL;
	}

	struPlaybackCmd *pCmd = m_lstPlaybackCmd.front();
	m_lstPlaybackCmd.pop_front();
	return pCmd;
}

void GSIOTClient::PlaybackCmd_clean()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackCmd.empty() )
	{
		return;
	}

	for( std::list<struPlaybackCmd*>::iterator it = m_lstPlaybackCmd.begin(); it!=m_lstPlaybackCmd.end(); ++it )
	{
		PlaybackCmd_DeleteCmd( *it );
	}

	m_lstPlaybackCmd.clear();
}

// return false : no work
bool GSIOTClient::PlaybackCmd_OnProc()
{
	struPlaybackCmd *pCmd = PlaybackCmd_pop();

	if( !pCmd )
	{
		return false;
	}

	PlaybackCmd_ProcOneCmd( pCmd );
	PlaybackCmd_DeleteCmd( pCmd );
	return true;
}

void GSIOTClient::Playback_ThreadCreate()
{
	m_PlaybackThreadCreateCount++;
	if( m_PlaybackThreadCreateCount > 99 )
	{
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "PlaybackProcThread Running(count=%d) limit error.", m_PlaybackThreadCreateCount );
		return;
	}

	m_isPlayBackThreadExit = false;

	LOGMSGEX( defLOGNAME, defLOG_SYS, "PlaybackProcThread Running(count=%d)...", m_PlaybackThreadCreateCount );
	HANDLE   hth1;
	unsigned  uiThread1ID;
	hth1 = (HANDLE)_beginthreadex( NULL, 0, PlaybackProcThread, this, 0, &uiThread1ID );
	CloseHandle(hth1);
}

void GSIOTClient::Playback_ThreadCheck()
{
	bool check_timeout = false;
	
	m_mutex_lstPlaybackList.lock();

	if( timeGetTime()-m_PlaybackThreadTick > 60000 )
	{
		check_timeout = true;
	}

	m_mutex_lstPlaybackList.unlock();

	if( check_timeout )
	{
		Playback_ThreadCreate();
	}
}

void GSIOTClient::Playback_ThreadPrinthb()
{
	LOGMSG( "PlaybackProcThread heartbeat(threadcount=%d)", m_PlaybackThreadCreateCount );
}

void GSIOTClient::Playback_DeleteDevOne( GSIOTDevice *device )
{
	((IPCameraBase*)(device->getControl()))->OnDisconnct();
	((IPCameraBase*)(device->getControl()))->StopRTMPSendAll();
	delete device;
}

bool GSIOTClient::Playback_IsLimit()
{
	const int PlaybackChannelLimit = RUNCODE_Get(defCodeIndex_SYS_PlaybackChannelLimit);

	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackList.size() >= PlaybackChannelLimit )
	{
		return true;
	}

	return false;
}

uint32_t GSIOTClient::Playback_GetNowCount()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	return m_lstPlaybackList.size();
}

void GSIOTClient::Playback_GetInfoList( std::map<std::string,struPlaybackSession> &getlstPlaybackList )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string, struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		it->second.lastUpdateTS = ((IPCameraBase*)(it->second.dev->getControl()))->GetSessionLastUpdateTime( c_NullStr );
		it++;
	}

	getlstPlaybackList = m_lstPlaybackList;
}

bool GSIOTClient::Playback_Exist( const std::string &key )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackList.find( key ) != m_lstPlaybackList.end() )
	{
		return true;
	}

	return false;
}

bool GSIOTClient::Playback_Add( const std::string &from_id, const std::string &key, const std::string &url, const std::string &peerid, const std::string &streamid, GSIOTDevice *device )
{
	if( !device )
		return false;

	if( !device->getControl() )
		return false;

	if( Playback_IsLimit() )
		return false;

	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackList.find( key ) != m_lstPlaybackList.end() )
	{
		return false;
	}

	m_lstPlaybackList[key] = struPlaybackSession(key, from_id, url, peerid, streamid, device->getName(), device);
	((IPCameraBase*)(device->getControl()))->UpdateSession( JID(defPlayback_SessionName) );
	return true;
}

void GSIOTClient::Playback_Delete( const std::string &key )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.find( key );
	if( it != m_lstPlaybackList.end() )
	{
		Playback_DeleteDevOne( it->second.dev );
		m_lstPlaybackList.erase( it );
	}
}

void GSIOTClient::Playback_DeleteForJid( const std::string &from_jid )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		if( from_jid == it->second.from_jid )
		{
			Playback_DeleteDevOne( it->second.dev );
			m_lstPlaybackList.erase( it );
			it = m_lstPlaybackList.begin();
		}
		else
		{
			it++;
		}
	}
}

void GSIOTClient::Playback_DeleteAll()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackList.empty() )
		return ;

	LOGMSG( "Playback_DeleteAll" );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it!=m_lstPlaybackList.end() )
	{
		Playback_DeleteDevOne( it->second.dev );
		m_lstPlaybackList.erase(it);
		it = m_lstPlaybackList.begin();
	}
}

void GSIOTClient::Playback_SetForJid( const std::string &from_jid, int sound )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		if( from_jid == it->second.from_jid )
		{
			if( sound>=0 )
			{
				LOGMSG( "Playback_SetForJid sound=%d, from=\"%s\"\r\n", sound, from_jid.c_str() );
				((IPCameraBase*)(it->second.dev->getControl()))->GetStreamObj()->GetRTMPSendObj()->set_playback_sound( sound );
			}

			break;
		}
		else
		{
			it++;
		}
	}
}

GSPlayBackCode_ GSIOTClient::Playback_CtrlForJid( const std::string &from_jid, GSPlayBackCode_ ControlCode, void *pInBuffer, uint32_t InLen, void *pOutBuffer, uint32_t *pOutLen )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		if( from_jid == it->second.from_jid )
		{
			LOGMSG( "Playback_CtrlForJid Ctrl=%d, from=\"%s\"", ControlCode, from_jid.c_str() );

			if( GSPlayBackCode_GetState != ControlCode )
			{
				PlayBackControl_nolock( (IPCameraBase*)(it->second.dev->getControl()), ControlCode, pInBuffer, InLen, pOutBuffer, pOutLen );
			}

			if( pOutBuffer )
			{
				*(int*)pOutBuffer = ((IPCameraBase*)(it->second.dev->getControl()))->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_speedlevel();
			}

			return ((IPCameraBase*)(it->second.dev->getControl()))->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_Code();
		}
		else
		{
			it++;
		}
	}

	return GSPlayBackCode_Stop;
}

void GSIOTClient::Playback_CtrlResult( const JID &from_Jid, const std::string &from_id, const XmppGSPlayback *pXmppSrc, const GSPlayBackCode_ ControlCode, void *pOutBuffer, uint32_t *pOutLen )
{
	XmppGSPlayback::defPBState state = XmppGSPlayback::defPBState_Unknown;
	switch( ControlCode )
	{
	case GSPlayBackCode_Stop:
		{
			state = XmppGSPlayback::defPBState_Stop;
			break;
		}

	case GSPlayBackCode_PLAYPAUSE:
		{
			state = XmppGSPlayback::defPBState_Pause;
			break;
		}

	case GSPlayBackCode_PLAYFAST:
		{
			if( pOutBuffer )
			{
				int speedlevel = *(int*)pOutBuffer;
				if( 2==speedlevel )
				{
					state = XmppGSPlayback::defPBState_FastPlay2;
					break;
				}
				else if( 1==speedlevel )
				{
					state = XmppGSPlayback::defPBState_FastPlay1;
					break;
				}
			}

			state = XmppGSPlayback::defPBState_FastPlay;
			break;
		}

	case GSPlayBackCode_PLAYSLOW:
		{

			if( pOutBuffer )
			{
				int speedlevel = *(int*)pOutBuffer;
				if( -2==speedlevel )
				{
					state = XmppGSPlayback::defPBState_SlowPlay2;
					break;
				}
				else if( -1==speedlevel )
				{
					state = XmppGSPlayback::defPBState_SlowPlay1;
					break;
				}
			}

			state = XmppGSPlayback::defPBState_SlowPlay;
			break;
		}

	case GSPlayBackCode_PLAYNORMAL:
	default:
		{
			state = XmppGSPlayback::defPBState_NormalPlay;
			break;
		}
	}

	IQ re( IQ::Result, from_Jid, from_id);
	re.addExtension( new XmppGSPlayback( struTagParam(), 
		pXmppSrc->get_camera_id(), pXmppSrc->get_url(), pXmppSrc->get_peerid(), pXmppSrc->get_streamid(), pXmppSrc->get_url(), state // key use url
		) );
	XmppClientSend(re,"handleIq Send(Playback_CtrlResult)");
}

//test
int GSIOTClient::PlayBackControl_GetCurState_test( GSPlayBackCode_ &curPB_Code, int &curPB_speedlevel, int &curPB_ThrowFrame )
{
	curPB_Code = GSPlayBackCode_NULL;
	curPB_speedlevel = 0;
	curPB_ThrowFrame = 0;

	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		LOGMSG( "PlayBackControl_GetCurState_test found\r\n" );
		IPCameraBase *pcam = (IPCameraBase*)(it->second.dev->getControl());
		curPB_Code = pcam->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_Code();
		curPB_speedlevel = pcam->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_speedlevel();
		curPB_ThrowFrame = pcam->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_ThrowFrame();
		return 1;
	}

	LOGMSG( "PlayBackControl_GetCurState_test not found\r\n" );
	return -1;
}

//test
int GSIOTClient::PlayBackControl_test( GSPlayBackCode_ ControlCode, void *pInBuffer, uint32_t InLen, void *pOutBuffer, uint32_t *pOutLen )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it != m_lstPlaybackList.end() )
	{
		int ret = 1;
		if( GSPlayBackCode_GetState != ControlCode )
		{
			ret = this->PlayBackControl_nolock( (IPCameraBase*)(it->second.dev->getControl()), ControlCode, pInBuffer, InLen, pOutBuffer, pOutLen );
		}

		LOGMSG( "PlayBackControl_test found state=%d\r\n", ((IPCameraBase*)(it->second.dev->getControl()))->GetStreamObj()->GetRTMPSendObj()->GetPlayBackCtrl_Code() );

		return ret;
	}

	LOGMSG( "PlayBackControl_test not found\r\n" );
	return -1;
}

int GSIOTClient::PlayBackControl_nolock( IPCameraBase *pcam, GSPlayBackCode_ ControlCode, void *pInBuffer, uint32_t InLen, void *pOutBuffer, uint32_t *pOutLen )
{
	LOGMSG( "PlayBackControl_nolock(%s) code=%d(%s), InValue=%u\r\n", pcam->GetName().c_str(), ControlCode, get_GSPlayBackCode_Name(ControlCode).c_str(), (uint32_t)pInBuffer );

	switch( ControlCode )
	{
	case GSPlayBackCode_SkipTime:
	case GSPlayBackCode_PLAYFAST:
	case GSPlayBackCode_PLAYSLOW:
	case GSPlayBackCode_PLAYNORMAL:
	case GSPlayBackCode_PLAYPAUSE:
	case GSPlayBackCode_PLAYRESTART:
		{
			return pcam->GetStreamObj()->GetRTMPSendObj()->PlayBackControl( ControlCode, (uint32_t)pInBuffer );
		}
		break;

	default:
		{
			int ret = pcam->PlayBackControl( ControlCode, pInBuffer, InLen, pOutBuffer, pOutLen );
			if( ret < 0 )
			{
				LOGMSG( "PlayBackControl_nolock failed\r\n" );
			}
			else
			{
				LOGMSG( "PlayBackControl_nolock success\r\n" );

				if( GSPlayBackCode_PlaySetTime == ControlCode 
					|| GSPlayBackCode_PLAYSETPOS == ControlCode
					)
				{
					pcam->GetStreamObj()->GetRTMPSendObj()->ClearListVideoPacket();
					//pcam->GetStreamObj()->GetRTMPSendObj()->ReKeyListVideoPacket();
				}
			}

			return ret;
		}
		break;
	}

	return -1;
}

void GSIOTClient::Playback_UpdateSession( const std::string &key )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.find( key );
	if( it != m_lstPlaybackList.end() )
	{
		((IPCameraBase*)(it->second.dev->getControl()))->UpdateSession( JID(defPlayback_SessionName) );
	}
}

void GSIOTClient::Playback_CheckSession()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstPlaybackList );

	if( m_lstPlaybackList.empty() )
		return ;

	LOGMSG( "Playback_CheckSession, SessionCount=%d", m_lstPlaybackList.size() );

	std::map<std::string,struPlaybackSession>::iterator it = m_lstPlaybackList.begin();
	while( it!=m_lstPlaybackList.end() )
	{
		((IPCameraBase*)(it->second.dev->getControl()))->CheckSession( 1, true, false );

		if( ((IPCameraBase*)(it->second.dev->getControl()))->GetSessionCount() <=0 )
		{
			LOGMSG( "Playback Session Timeout, devid=%d, devname=%s", it->second.dev->getId(), it->second.dev->getName().c_str() );

			Playback_DeleteDevOne( it->second.dev );
			m_lstPlaybackList.erase(it);
			it = m_lstPlaybackList.begin();
			continue;
		}

		uint32_t pos = -1;
		uint32_t outlen = sizeof(pos);
		((IPCameraBase*)(it->second.dev->getControl()))->PlayBackControl( GSPlayBackCode_PLAYGETPOS, 0, 0, &pos, &outlen );

		if( pos<0 || pos >= 100 )
		{
			// notice stop
			JID to_jid(it->second.from_jid);
			IQ re( IQ::Result, to_jid );
			re.addExtension( new XmppGSPlayback( struTagParam(), 
				it->second.dev->getId(), it->second.url, it->second.peerid, it->second.streamid, it->first, XmppGSPlayback::defPBState_Stop // key use url
				) );
			XmppClientSend(re,"handleIq Send(Playback_CheckSession Play end notice)");

			// delete
			Playback_DeleteDevOne( it->second.dev );
			m_lstPlaybackList.erase(it);
			it = m_lstPlaybackList.begin();
			continue;
		}

		if( !it->second.check() )
		{
			// notice stop
			JID to_jid( it->second.from_jid );
			IQ re( IQ::Result, to_jid );
			re.addExtension( new XmppGSPlayback( struTagParam(),
				it->second.dev->getId(), it->second.url, it->second.peerid, it->second.streamid, it->first, XmppGSPlayback::defPBState_Stop // key use url
				) );
			XmppClientSend( re, "handleIq Send(Playback_CheckSession Play overtime notice)" );

			// delete
			Playback_DeleteDevOne( it->second.dev );
			m_lstPlaybackList.erase( it );
			it = m_lstPlaybackList.begin();
			continue;
		}

		++it;
	}
}

bool GSIOTClient::EventNoticeMsg_Add( struEventNoticeMsg *msg )
{
	if( !msg )
		return false;

	gloox::util::MutexGuard mutexguard( m_mutex_lstEventNoticeMsg );

	if( m_lstEventNoticeMsg.find( msg->id ) != m_lstEventNoticeMsg.end() )
	{
		return false;
	}

	m_lstEventNoticeMsg[msg->id] = msg;

	return true;
}

void GSIOTClient::EventNoticeMsg_Remove( const std::string &id )
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstEventNoticeMsg );

	std::map<std::string,struEventNoticeMsg*>::iterator it = m_lstEventNoticeMsg.find( id );
	if( it != m_lstEventNoticeMsg.end() )
	{
		delete it->second;
		m_lstEventNoticeMsg.erase( it );
	}
}

void GSIOTClient::EventNoticeMsg_Check()
{
	gloox::util::MutexGuard mutexguard( m_mutex_lstEventNoticeMsg );

	if( m_lstEventNoticeMsg.empty() )
		return ;

	LOGMSG( "EventNoticeMsg_Check, Count=%d", m_lstEventNoticeMsg.size() );

	std::map<std::string,struEventNoticeMsg*>::iterator it = m_lstEventNoticeMsg.begin();
	while( it!=m_lstEventNoticeMsg.end() )
	{
		if( ::timeGetTime() - it->second->starttime >= 10000 )
		{
			LOGMSG( "EventNoticeMsg Timeout, id=%s, to_jid=%s, Subject=%s", it->second->id.c_str(), it->second->to_jid.full().c_str(), it->second->strSubject.c_str() );

			// sent to server
#if 1
			EventNoticeMsg_Send( it->second->to_jid.full(), it->second->strSubject, it->second->strBody, "handleIq Send(EventNoticeMsg_Check Timeout, sendto svr)" );
#else
			IQ re( IQ::Set, JID("webservice@gsss.cn/iotserver-side") );
			re.addExtension( new XmppGSMessage( struTagParam(), it->second->to_jid.full(), it->second->strSubject, it->second->strBody ) );
			XmppClientSend(re,"handleIq Send(EventNoticeMsg_Check Timeout, sendto svr)");
#endif

			// erase
			delete it->second;
			m_lstEventNoticeMsg.erase(it);
			it = m_lstEventNoticeMsg.begin();
			continue;
		}
		
		++it;
	}
}

void GSIOTClient::EventNoticeMsg_Send( const std::string &tojid, const std::string &subject, const std::string &body, const char *callinfo )
{
	IQ re( IQ::Set, JID("webservice@gsss.cn/iotserver-side") );
	re.addExtension( new XmppGSMessage( struTagParam(), tojid, subject, body ) );
	XmppClientSend( re, callinfo );
}

bool GSIOTClient::Update_Check( std::string &strVerLocal, std::string &strVerNew )
{
	m_retCheckUpdate = false;
	strVerLocal = "";
	strVerNew = "";
	m_strVerLocal = "";
	m_strVerNew = "";
	std::string strPath = getAppPath();

	// 获取本地版本
	char bufval[256] = {0};
	const DWORD nSize = sizeof(bufval);
	DWORD ReadSize = ::GetPrivateProfileStringA( "sys", "version", "0", 
		bufval,
		nSize,
		(std::string(defFilePath)+"\\version.ini").c_str()
		);

	m_strVerLocal = bufval;

	std::string reqstr = "http://api.gsss.cn/iot_ctl_update.ashx";

	httpreq::Request req_setjid;
	std::string psHeaderSend;
	std::string psHeaderReceive;
	std::string psMessage;
	if( !req_setjid.SendRequest( false, reqstr.c_str(), psHeaderSend, psHeaderReceive, psMessage ) )//?ver=20131200
	{
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "SendRequest to server send failed." );
		return false;
	}

	LOGMSGEX( defLOGNAME, defLOG_INFO, "SendRequest to server send success. HeaderReceive=\"%s\", Message=\"%s\"", UTF8ToASCII(psHeaderReceive).c_str(), UTF8ToASCII(psMessage).c_str() );

	std::string copy = psMessage;
	int ret = 0;
	if( ( ret = m_parser.feed( copy ) ) >= 0 )
	{
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "parser err. ret=%d", ret );
		return false;
	}

	strVerLocal = m_strVerLocal;
	strVerNew = m_strVerNew;

	return m_retCheckUpdate;
}

void GSIOTClient::Update_Check_fromremote( const JID &fromjid, const std::string& from_id )
{
	sg_blUpdatedProc++;
	std::string strVerLocal;
	std::string strVerNew;
	XmppGSUpdate::defUPState state = XmppGSUpdate::defUPState_Unknown;

	if( this->Update_Check( strVerLocal, strVerNew ) )
	{
		if( strVerLocal == strVerNew )
		{
			state = XmppGSUpdate::defUPState_latest;
		}
		else
		{
			state = XmppGSUpdate::defUPState_update;
		}
	}
	else
	{
		state = XmppGSUpdate::defUPState_checkfailed;
	}

	IQ re( IQ::Result, fromjid, from_id );
	re.addExtension( new XmppGSUpdate( struTagParam(), 
		strVerLocal, strVerNew, state
		) );
	XmppClientSend(re,"handleIq Send(ExtIotUpdate check ACK)");
}

void GSIOTClient::handleTag( Tag* tag )
{
	std::string strPath = getAppPath();

	Tag *tmgr = tag;
	if( tag->name() != "update" )
	{
		LOGMSGEX( defLOGNAME, defLOG_INFO, "not fond update tag." );
		return;
	}

	if( tmgr->findChild("version") )
	{
		m_strVerNew = tmgr->findChild("version")->cdata();
	}
	else
	{
		LOGMSGEX( defLOGNAME, defLOG_ERROR, "not get new ver!" );
		return;
	}

	m_retCheckUpdate = true;

	LOGMSGEX( defLOGNAME, defLOG_INFO, "localver=%s, newver=%s", m_strVerLocal.c_str(), m_strVerNew.c_str() );
}

bool GSIOTClient::Update_DoUpdateNow( uint32_t &err, std::string runparam )
{
	err = 0;
	LOGMSGEX( defLOGWatch, defLOG_SYS, "执行程序升级...\r\n" );
	LOGMSGEX( defLOGNAME, defLOG_SYS, "run update" );
	HINSTANCE h = ShellExecuteA( NULL, "open", (std::string(defFilePath)+"\\"+defFileName_Update).c_str(), runparam.c_str(), NULL, SW_SHOWNORMAL );
	if( (uint32_t)h > 32 )
		return true;

	err = (uint32_t)h;
	return false;
}

void GSIOTClient::Update_DoUpdateNow_fromremote( const JID &fromjid, const std::string& from_id, std::string runparam )
{
	// 写入信息
	WritePrivateProfileStringA(
		"update",
		"state",
		"update",
		(std::string(defFilePath)+"\\temp.ini").c_str()
		);

	WritePrivateProfileStringA(
		"update",
		"fromjid",
		fromjid.full().c_str(),
		(std::string(defFilePath)+"\\temp.ini").c_str()
		);

	uint32_t err = 0;

	if( this->Update_DoUpdateNow( err, runparam ) )
	{
		IQ re( IQ::Result, fromjid, from_id);
		re.addExtension( new XmppGSUpdate( struTagParam(), 
			"", "", XmppGSUpdate::defUPState_progress
			) );
		XmppClientSend(re,"handleIq Send(ExtIotUpdate update ACK)");
	}
	else
	{
		IQ re( IQ::Result, fromjid, from_id);
		re.addExtension( new XmppGSUpdate( struTagParam(), 
			"", "", XmppGSUpdate::defUPState_updatefailed
			) );
		XmppClientSend(re,"handleIq Send(ExtIotUpdate update ACK)");
	}
}

// 更新完成处理
void GSIOTClient::Update_UpdatedProc()
{
	if( sg_blUpdatedProc )
	{
		return;
	}

	static uint32_t st_Update_UpdatedProc = timeGetTime();
	if( timeGetTime()-st_Update_UpdatedProc < 5000 )
		return;

	sg_blUpdatedProc++;

	char bufval[256] = {0};
	const DWORD nSize = sizeof(bufval);
	DWORD ReadSize = ::GetPrivateProfileStringA( 
		"update", 
		"state", "", 
		bufval,
		nSize,
		(std::string(defFilePath)+"\\temp.ini").c_str()
		);
	
	// 重置
	if( ReadSize>0 )
	{
		WritePrivateProfileStringA(
			"update",
			"state",
			"",
			(std::string(defFilePath)+"\\temp.ini").c_str()
			);
	}

	if( std::string("success updated") != bufval )
	{
		return;
	}

	char bufval_fromjid[256] = {0};
	const DWORD nSize_fromjid = sizeof(bufval_fromjid);
	ReadSize = ::GetPrivateProfileStringA( 
		"update", 
		"fromjid", "", 
		bufval_fromjid,
		nSize_fromjid,
		(std::string(defFilePath)+"\\temp.ini").c_str()
		);

	std::string fromjid = bufval_fromjid;
	//fromjid = "chen009@gsss.cn";//test
	if( fromjid.empty() )
	{
		return;
	}

	// 重置
	if( ReadSize>0 )
	{
		WritePrivateProfileStringA(
			"update",
			"fromjid",
			"",
			(std::string(defFilePath)+"\\temp.ini").c_str()
			);
	}

	IQ re( IQ::Set, fromjid );
	re.addExtension( new XmppGSUpdate( struTagParam(), 
		"", "", XmppGSUpdate::defUPState_successupdated
		) );
	XmppClientSend(re,"handleIq Send(ExtIotUpdate successupdated ACK)");

//#if 1
//	Message msg( Message::Normal, fromjid, ASCIIToUTF8("更新完成。"), ASCIIToUTF8(std::string(XMPP_MESSAGE_PREHEAD)+"更新完成") );
//	XmppPrint( msg, "Update_UpdatedProc()" );
//	xmppClient->send( msg );
//#else
//	IQ re( IQ::Result, fromjid );
//	re.addExtension( new XmppGSUpdate( struTagParam(), 
//		"", "", XmppGSUpdate::defUPState_successupdated
//		) );
//	XmppClientSend(re,"handleIq Send(ExtIotUpdate successupdated ACK)");
//#endif
}

void* GSIOTClient::OnTalkNotify( const XmppGSTalk::defTalkCmd cmd, const std::string &url, const std::string &from_Jid, const std::string &from_id, bool isSyncReturn, const defvecDevKey &vecdev, bool result, IOTDeviceType getdev_type, int getdev_id )
{
	switch( cmd )
	{
	case XmppGSTalk::defTalkCmd_session:	// 会话
		{
			IQ re( IQ::Result, JID(from_Jid), from_id );
			re.addExtension( new XmppGSTalk( struTagParam(), 
				XmppGSTalk::defTalkCmd_session, url, vecdev ) );
			XmppClientSend( re, "OnTalkReturn session success" );
		}
		break;

	case XmppGSTalk::defTalkCmd_adddev:	// 增加对讲设备
		{
			IQ re( isSyncReturn?IQ::Result:IQ::Set, JID(from_Jid), isSyncReturn?from_id:"" );
			re.addExtension( new XmppGSTalk( struTagParam(), 
				XmppGSTalk::defTalkCmd_adddev,  url, vecdev, result ) );
			XmppClientSend( re, "OnTalkReturn adddev" );
		}
		break;

	case XmppGSTalk::defTalkCmd_removedev:	// 移除对讲设备
		{
			IQ re( isSyncReturn?IQ::Result:IQ::Set, JID(from_Jid), isSyncReturn?from_id:"" );
			re.addExtension( new XmppGSTalk( struTagParam(), 
				XmppGSTalk::defTalkCmd_removedev,  url, vecdev, result ) );
			XmppClientSend( re, "OnTalkReturn removedev" );
		}
		break;
		
	case XmppGSTalk::defTalkCmd_keepalive:	// 心跳
		{
			IQ re( isSyncReturn?IQ::Result:IQ::Set, JID(from_Jid), isSyncReturn?from_id:"" );
			re.addExtension( new XmppGSTalk( struTagParam(), 
				XmppGSTalk::defTalkCmd_keepalive,  url, vecdev, result ) );
			XmppClientSend( re, "OnTalkReturn keepalive" );
		}
		break;

	case XmppGSTalk::defTalkCmd_quit:		// 结束
		{
#if 1
			IQ re( isSyncReturn?IQ::Result:IQ::Set, JID(from_Jid), isSyncReturn?from_id:"" );
			re.addExtension( new XmppGSTalk( struTagParam(), 
				XmppGSTalk::defTalkCmd_quit, url, vecdev, result ) );
			XmppClientSend( re, "OnTalkReturn quit ret/set" );
#else
			if( isSyncReturn )
			{
				IQ re( IQ::Result, JID(from_Jid), from_id );
				re.addExtension( new XmppGSTalk( struTagParam(), 
					XmppGSTalk::defTalkCmd_quit, url, vecdev, result ) );
				XmppClientSend( re, "OnTalkReturn quit isSyncReturn" );
			}
			else
			{
				Message msg( Message::Normal, JID(from_Jid), ASCIIToUTF8(url), ASCIIToUTF8(std::string(XMPP_MESSAGE_PREHEAD)+"TalkQuit") );
				XmppPrint( msg, "OnTalkReturn quit msg" );
				xmppClient->send( msg );
			}
#endif
		}
		break;
		
		// 内部命令
	case XmppGSTalk::defTalkSelfCmd_GetDevice:
		{
			if( IOT_DEVICE_Camera == getdev_type )
			{
				return ipcamClient->GetIOTDevice( getdev_id );
			}

			return NULL; // 暂不支持其它设备
		}
		break;

	default:
		{
			LOGMSGEX( defLOGNAME, defLOG_ERROR, "OnTalkReturn unsupport cmd=%d!!!" );
		}
		break;
	}

	return NULL;
}

void GSIOTClient::DataProc_ThreadCreate()
{
	if( IsRUNCODEEnable(defCodeIndex_Dis_RunDataProc) )
	{
		return ;
	}

	m_isDataProcThreadExit = false;

	HANDLE   hth1;
	unsigned  uiThread1ID;
	hth1 = (HANDLE)_beginthreadex( NULL, 0, DataProcThread, this, 0, &uiThread1ID );
	CloseHandle(hth1);
}

// 处理数据采集发送逻辑
bool GSIOTClient::DataProc()
{
	static uint32_t s_DataProc_Polling_count = 1;
	s_DataProc_Polling_count++;

#if defined(defForceDataSave)
	if( IsRUNCODEEnable(defCodeIndex_TEST_ForceDataSave) )
	{
		LOGMSG( "DataProc Polling ForceDataSave!" );
	}
	else
#endif
	{
		if( 0==(s_DataProc_Polling_count%5) )
		{
			LOGMSG( "DataProc Polling(5n)" );
		}
	}

	const time_t curUTCTime = g_GetUTCTime();

	const bool LED_Enable = IsRUNCODEEnable( defCodeIndex_LED_Config );
	const int LED_Mod = RUNCODE_Get( defCodeIndex_LED_Config, defRunCodeValIndex_2 );
	const uint32_t LED_ValueOvertime = RUNCODE_Get( defCodeIndex_LED_ValueOvertime );
	const bool LED_ValueOvertime_Show = RUNCODE_Get( defCodeIndex_LED_ValueOvertime, defRunCodeValIndex_2 );
	bool hasUpdateLedShow = false;
	int LedShowMaxCount = 0;
	std::map<std::string,struLEDShow> lstLEDShow; //<sortkey,strshow>

	std::list<GSIOTDevice*>::const_iterator it = IotDeviceList.begin();
	for( ; m_running && it!=IotDeviceList.end(); ++it )
	{
		const GSIOTDevice *iotdevice = (*it);

		if( !iotdevice->GetEnable() )
		{
			continue;
		}

		if( !iotdevice->getControl() )
		{
			continue;
		}

		ControlBase *control = iotdevice->getControl();
		switch( control->GetType() )
		{
		case IOT_DEVICE_RS485:
			{
				const defUseable useable = iotdevice->get_all_useable_state();

				RS485DevControl *ctl = (RS485DevControl*)control;
				if( ctl )
				{
					bool doSendDev = false;

					const defAddressQueue& AddressList = ctl->GetAddressList();
					std::list<DeviceAddress*>::const_iterator it = AddressList.begin();
					for( ; m_running && it!=AddressList.end(); ++it )
					{
						DeviceAddress *address = *it;
						
						if( !address->GetEnable() )
							continue;

						if( !g_isNeedSaveType(address->GetType()) )
							continue;

						const GSIOTAddrObjKey AddrObjKey( iotdevice->getType(), iotdevice->getId(), address->GetType(), address->GetAddress() );

						if( m_DataStoreMgr->insertdata_CheckSaveInvalid( AddrObjKey, useable>0 ) )
						{
							gloox::util::MutexGuard mutexguard( m_mutex_DataStore );

							const size_t DataSaveBufSize = m_lstDataSaveBuf.size();
							if( DataSaveBufSize<10000 )
							{
								m_lstDataSaveBuf.push_back( new struDataSave( g_GetUTCTime(), iotdevice->getType(), iotdevice->getId(), address->GetType(), address->GetAddress(), defDataFlag_Invalid, c_ZeroStr, c_NullStr ) );
							}
							else if( DataSaveBufSize > 100 )
							{
								LOGMSG( "lstDataSaveBuf max, size=%d", m_lstDataSaveBuf.size() );
							}
						}

						// 轮询时间
						int polltime = 60*1000;

						switch( address->GetType() )
						{
						case IOT_DEVICE_Wind:
							{
								polltime = 6*1000;
							}
							break;

						case IOT_DEVICE_Temperature:
						case IOT_DEVICE_Humidity:
						default:
							{
								polltime = 60*1000;
							}
							break;
						}

#if defined(defForceDataSave)
						if( IsRUNCODEEnable(defCodeIndex_TEST_ForceDataSave) )
						{
							polltime = 8*1000;
						}
#endif

						switch( address->GetType() )
						{
						case IOT_DEVICE_Temperature:
						case IOT_DEVICE_Humidity:
						case IOT_DEVICE_Wind:
							{
								if( LED_Enable )
								{
									LedShowMaxCount++;
									bool isLongOld = false;
									uint32_t noUpdateTime = 0;
									const std::string strCurValue = address->GetCurValue( &isLongOld, &noUpdateTime, LED_ValueOvertime*1000 );

									if( !isLongOld )
									{
										lstLEDShow[AddrObjKey.get_str(true,iotdevice->getName())] = struLEDShow( address->GetName(), strCurValue+g_GetUnitBaseForType(address->GetType()) );

										if( noUpdateTime<10000 )
										{
											hasUpdateLedShow = true;
										}
									}
									else if( LED_ValueOvertime_Show )
									{
										lstLEDShow[AddrObjKey.get_str(true,iotdevice->getName())] = struLEDShow( address->GetName(), RUNCODE_GetStr(defCodeIndex_LED_ValueOvertime) );
									}
								}

								if( doSendDev )
								{
									continue;
								}

								const int MultiReadCount = address->PopMultiReadCount();
								bool isOld = ( MultiReadCount > 0 );
								uint32_t noUpdateTime = 0;
								bool isLowSampTime = false;

								const bool curisTimePoint = g_isTimePoint(curUTCTime,address->GetType());

								if( !isOld )
								{
									const std::string strCurValue = address->GetCurValue( &isOld, &noUpdateTime, polltime, &isLowSampTime, curisTimePoint );
								}

								if( !isLowSampTime )
								{
									if( !isOld )
									{
										isOld = ( curisTimePoint && g_TransToTimePoint(curUTCTime, address->GetType(), false)!=g_TransToTimePoint(address->GetLastSaveTime(), address->GetType(), true) ); // 在时间点才缩短采集间隔
									}
								}

								// 值过旧未更新，或者当前在时间点上，并且当前时间点未存储
								if( isOld )
								{
									GSIOTDevice *sendDev = iotdevice->clone( false );
									RS485DevControl *sendctl = (RS485DevControl*)sendDev->getControl();
									if( sendctl )
									{
										if( IsRUNCODEEnable(defCodeIndex_SYS_DataSamp_DoBatch) )
										{
											doSendDev = sendctl->IsCanBtachRead();
										}

										address->NowSampTick();
										sendctl->SetCommand( defModbusCmd_Read );
										this->SendControl( iotdevice->getType(), sendDev, doSendDev?NULL:address );

										LOGMSG( "Polling(%s) MRead=%d : dev(%d,%s) addr(%d%s)", curisTimePoint?"TimePoint":"", MultiReadCount, iotdevice->getId(), iotdevice->getName().c_str(), doSendDev?0:address->GetAddress(), doSendDev?"all":"" );
									}

									macCheckAndDel_Obj(sendctl);
								}
							}
							break;
						}

						//if( doSendDev )
						//{
						//	break;
						//}
					}
				}
			}
			break;
		}
	}

	if( LED_Enable )
	{
		const uint32_t lastShowLedTime = timeGetTime()-m_lastShowLedTick;
		const uint32_t LED_ShowInterval = RUNCODE_Get(defCodeIndex_LED_ShowInterval);
		const uint32_t LED_ShowInterval_min = RUNCODE_Get(defCodeIndex_LED_ShowInterval,defRunCodeValIndex_2);

		if( hasUpdateLedShow )
		{
			if( g_GetUTCTime()-m_IOT_starttime < 30 )
			{
				hasUpdateLedShow = LedShowMaxCount==lstLEDShow.size();
			}
		}

		if( (hasUpdateLedShow && lastShowLedTime>(LED_ShowInterval_min*1000))
			|| lastShowLedTime>(LED_ShowInterval*1000)
			)
		{
			if( LEDFunc_CheckReady() < 0 )
			{
				LEDFunc_Init( RUNCODE_GetStr(defCodeIndex_LED_DevParam).c_str(), RUNCODE_Get(defCodeIndex_LED_DevParam) );
			}

			const int nullline = RUNCODE_Get( defCodeIndex_LED_Format, defRunCodeValIndex_2 );

			std::string strLedShow = RUNCODE_GetStr( defCodeIndex_LED_Title );
			for( std::map<std::string,struLEDShow>::const_iterator it=lstLEDShow.begin(); it!=lstLEDShow.end(); ++it )
			{
				if( !strLedShow.empty() )
					strLedShow += nullline?"\n\n":"\n";

				strLedShow += it->second.name;
				strLedShow += ": ";
				strLedShow += it->second.value;
			}

#ifdef _DEBUG
			LOGMSG( "LED Show: \n%s", strLedShow.c_str() );
#endif
			g_replace_all_distinct( strLedShow, "%%", "%" );
			LEDFunc_ShowText( strLedShow.c_str() );

			m_lastShowLedTick = timeGetTime();
		}
	}

	return true;
}

void GSIOTClient::DataSave()
{
	defvecDataSave vecDataSave;
	const DWORD dwStart = ::timeGetTime();
	while( m_DataStoreMgr )
	{
		DataSaveBuf_Pop( vecDataSave );
		if( vecDataSave.empty() )
		{
			break;
		}

		m_DataStoreMgr->insertdata( vecDataSave );
		g_delete_vecDataSave( vecDataSave );

		if( ::timeGetTime()-dwStart > 900 )
		{
			break;
		}
	}
}

void GSIOTClient::DataStatCheck()
{
	m_DataStoreMgr->CheckStat();
}

bool GSIOTClient::DataSaveBuf_Pop( defvecDataSave &vecDataSave )
{
	gloox::util::MutexGuard mutexguard( m_mutex_DataStore );

	if( m_lstDataSaveBuf.empty() )
		return false;

	struDataSave *p = m_lstDataSaveBuf.front();
	m_lstDataSaveBuf.pop_front();
	vecDataSave.push_back( p );
	const time_t curBatchTime = p->data_dt;

	while( !m_lstDataSaveBuf.empty() )
	{
		struDataSave *p = m_lstDataSaveBuf.front();

		if( !CDataStoreMgr::IsSameDBSave( curBatchTime, p->data_dt ) )
		{
			break;
		}

		m_lstDataSaveBuf.pop_front();
		vecDataSave.push_back( p );

		if( vecDataSave.size() >= 50 )
		{
			break;
		}
	}

	return true;
}

// 摄像机告警原始信息接收
void GSIOTClient::OnCameraAlarmRecv( const bool isAlarm, const IPCameraType CameraType, const char *sDeviceIP, const int nPort, const int channel, const char *alarmstr )
{
	if( IsRUNCODEEnable(defCodeIndex_SYS_CamAlarmDebugLog) )
	{
		LOGMSG( "OnCameraAlarmRecv(%s): isAlarm=%d, %s:%d:%d %s", g_ConvertCameraTypeToString(CameraType).c_str(), (int)isAlarm, sDeviceIP, nPort, channel, alarmstr );
	}

	if( isAlarm )
	{
		this->CameraAlarmRecvMap_push( CameraType, sDeviceIP, nPort, channel, alarmstr );
	}
}

// 摄像机告警原始信息待处理缓冲队列 加入
bool GSIOTClient::CameraAlarmRecvMap_push( const IPCameraType CameraType, const char *sDeviceIP, const int nPort, const int channel, const char *alarmstr )
{
	gloox::util::MutexGuard mutexguard( m_mutex_AlarmProc );

	if( m_lstCamAlarmRecv.size()>5000 )
	{
		static uint32_t lastLogMsg = timeGetTime()-3600*1000;

		if( timeGetTime()-lastLogMsg >30*1000 )
		{
			lastLogMsg = timeGetTime();
			LOGMSGEX( defLOGNAME, defLOG_ERROR, "CameraAlarmRecvMap_push limit=%d!!!\r\n", m_lstCamAlarmRecv.size() );
		}

		for( deflstCamAlarmRecv::const_iterator it=m_lstCamAlarmRecv.begin(); it!=m_lstCamAlarmRecv.end(); )
		{
			const struCamAlarmRecv &CamAlarmRecv = *it;

			if( CamAlarmRecv.isOverTime( timeGetTime() ) )
			{
				LOGMSGEX( defLOGNAME, defLOG_ERROR, "CameraAlarmRecvMap_push isOverTime(%s:%d:%d,%s)\r\n", CamAlarmRecv.key.sDeviceIP.c_str(), CamAlarmRecv.key.nPort, CamAlarmRecv.key.channel, CamAlarmRecv.alarmstr.c_str() );
				m_lstCamAlarmRecv.erase(it);
				it=m_lstCamAlarmRecv.begin();
				continue;
			}

			++it;
		}

		return false;
	}

	const struCamAlarmRecv_key key( sDeviceIP, nPort, channel );
	struCamAlarmRecv newCamAlarmRecv( timeGetTime(), key, CameraType, alarmstr );
	g_struGSTime_GetCurTime( newCamAlarmRecv.dt );

	for( deflstCamAlarmRecv::iterator it=m_lstCamAlarmRecv.begin(); it!=m_lstCamAlarmRecv.end(); ++it )
	{
		struCamAlarmRecv &curCamAlarmRecv = *it;

		if( newCamAlarmRecv.key == curCamAlarmRecv.key )
		{
			curCamAlarmRecv = newCamAlarmRecv;

			if( IsRUNCODEEnable(defCodeIndex_SYS_CamAlarmDebugLog) )
			{
				LOGMSG( "CameraAlarmRecvMap_push edit, after count=%d", m_lstCamAlarmRecv.size() );
			}

			return true;
		}
	}

	m_lstCamAlarmRecv.push_back( newCamAlarmRecv );

	if( IsRUNCODEEnable(defCodeIndex_SYS_CamAlarmDebugLog) )
	{
		LOGMSG( "CameraAlarmRecvMap_push back, after count=%d", m_lstCamAlarmRecv.size() );
	}

	return true;
}

// 摄像机告警原始信息待处理缓冲队列 获取
bool GSIOTClient::CameraAlarmRecvMap_pop( struCamAlarmRecv &CamAlarmRecv )
{
	gloox::util::MutexGuard mutexguard( m_mutex_AlarmProc );

	if( m_lstCamAlarmRecv.empty() )
	{
		return false;
	}
	
	deflstCamAlarmRecv::const_iterator it = m_lstCamAlarmRecv.begin();
	CamAlarmRecv = *it;
	m_lstCamAlarmRecv.erase(it);

	return true;
}

int GSIOTClient::CameraAlarmRecvMap_size()
{
	gloox::util::MutexGuard mutexguard( m_mutex_AlarmProc );

	return m_lstCamAlarmRecv.size();
}

//告警处理线程创建
void GSIOTClient::AlarmProc_ThreadCreate()
{
	m_isAlarmProcThreadExit = false;

	HANDLE   hth1;
	unsigned  uiThread1ID;
	hth1 = (HANDLE)_beginthreadex( NULL, 0, AlarmProcThread, this, 0, &uiThread1ID );
	CloseHandle(hth1);
}

//告警处理
bool GSIOTClient::AlarmProc()
{
	const DWORD dwStart = ::timeGetTime();
	bool isdo = true;
	while(isdo)
	{
		struCamAlarmRecv CamAlarmRecv;
		isdo = CameraAlarmRecvMap_pop( CamAlarmRecv );
		if( !isdo )
		{
			return false;
		}

		std::list<GSIOTDevice*> &cameraList = this->ipcamClient->GetCameraManager()->GetCameraList();
		std::list<GSIOTDevice*>::iterator it = cameraList.begin();
		for( ; it!=cameraList.end(); ++it )
		{
			GSIOTDevice *pDev = (*it);
			IPCameraBase *cam = (IPCameraBase*)(*it)->getControl();
			if( !cam )
				continue;

			if( CamAlarmRecv.key.sDeviceIP != cam->GetIPAddress() || CamAlarmRecv.key.nPort != cam->GetPort() )
				continue;

			if( !isInvalidCamCh(CamAlarmRecv.key.channel) )
			{
				if( CamAlarmRecv.key.channel != cam->GetChannel() )
					continue;
			}

			if( !pDev->GetEnable() )
				break;

			if( !cam->GetAdvAttr().get_AdvAttr(defCamAdvAttr_SupportAlarm) )
				break;

			if( m_EnableTriggerDebug && m_ITriggerDebugHandler )
			{
				m_ITriggerDebugHandler->OnTriggerDebug( defLinkID_Local, IOT_DEVICE_Camera, pDev->getName(), cam->GetAGRunState(), this->GetAlarmGuardGlobalFlag(), g_IsValidCurTimeInAlarmGuardState(), CamAlarmRecv.dt, pDev->GetStrAlmBody( true, CamAlarmRecv.dt, CamAlarmRecv.alarmstr ), pDev->GetStrAlmSubject( true ) );
			}

			const bool AGRunState = cam->GetAGRunState();
			if( !AGRunState )
				break;

			const int AlarmGuardGlobalFlag = this->GetAlarmGuardGlobalFlag();
			if( !AlarmGuardGlobalFlag )
				break;

			const bool IsValidCurTime = g_IsValidCurTimeInAlarmGuardState();
			if( !IsValidCurTime )
				break;

			const bool isCurChanged = cam->SetCurAlarmState( defAlarmState_NormAlarm );
			if( isCurChanged )
			{
				if( IsRUNCODEEnable(defCodeIndex_SYS_CamAlarmDebugLog) )
				{
					LOGMSG( "AlarmProc Change to AlarmStart(%s:%d:%d,%s)\r\n", CamAlarmRecv.key.sDeviceIP.c_str(), CamAlarmRecv.key.nPort, CamAlarmRecv.key.channel, CamAlarmRecv.alarmstr.c_str() );
				}

				this->DoAlarmDevice( pDev, AGRunState, AlarmGuardGlobalFlag, IsValidCurTime, pDev->GetStrAlmBody( true, CamAlarmRecv.dt, CamAlarmRecv.alarmstr ), pDev->GetStrAlmSubject( true ) );
			}

			break;
		}

		const DWORD usems = ::timeGetTime()-dwStart;
		if( usems > 5000 )
		{
			LOGMSG( "AlarmProc use=%dms longtime return, CameraAlarmRecvMap_size=%d", usems, CameraAlarmRecvMap_size() );
			return true;
		}
	}

	return isdo;
}

// 检查告警状态是否超时，而恢复告警
bool GSIOTClient::AlarmCheck()
{
	bool isChanged = false;

	std::list<GSIOTDevice*> &cameraList = this->ipcamClient->GetCameraManager()->GetCameraList();
	std::list<GSIOTDevice*>::iterator it = cameraList.begin();
	for( ; it!=cameraList.end(); ++it )
	{
		GSIOTDevice *pDev = (*it);
		IPCameraBase *cam = (IPCameraBase*)(*it)->getControl();
		if( !cam )
			continue;

		const bool isResume = cam->CheckResumeCurAlarmState( pDev->GetEnable() );
		isChanged |= isResume;

		if( isResume )
		{
			if( IsRUNCODEEnable(defCodeIndex_SYS_CamAlarmDebugLog) )
			{
				LOGMSG( "AlarmCheck AlarmResume(%s,%s:%d:%d)\r\n", pDev->getName().c_str(), cam->GetIPAddress().c_str(), cam->GetPort(), cam->GetChannel() );
			}
		}
	}

	return isChanged;
}

void GSIOTClient::check_all_NetUseable( const bool CheckNow )
{
	if( !IsRUNCODEEnable(defCodeIndex_SYS_AutoCheckNetUseable) )
	{
		return ;
	}

	bool doCheck = CheckNow;

	if( CheckNow )
	{
		m_last_checkNetUseable_camid = 0;
	}

	if( !doCheck && 0!=m_last_checkNetUseable_camid )
	{
		doCheck = true;
	}

	const uint32_t dwStart = timeGetTime();
	uint32_t timeMax = defcheckNetUseable_timeMax;
	if( !doCheck )
	{
		if( dwStart-m_last_checkNetUseable_time > timeMax )
		{
			doCheck = true;
		}
		//else
		//{
		//	const uint32_t timeLowMax = 60*1000;
		//	if( curtime-m_last_checkNetUseable_time > timeLowMax && hasNetUseableFailed() )
		//	{
		//		timeMax = timeLowMax;
		//		doCheck = true;
		//	}
		//}
	}

	if( !doCheck )
	{
		return;
	}

	uint32_t thisCheckCount = 0;
	std::list<GSIOTDevice*> &cameraList = this->ipcamClient->GetCameraManager()->GetCameraList();
	std::list<GSIOTDevice*>::const_iterator it = cameraList.begin();
	for( ; it!=cameraList.end(); ++it )
	{
		if( m_last_checkNetUseable_camid )
		{
			if( (*it)->getId() == m_last_checkNetUseable_camid )
			{
				m_last_checkNetUseable_camid = 0;
			}

			continue;
		}

		IPCameraBase *cam = (IPCameraBase*)(*it)->getControl();
		if(cam)
		{
			if( (*it)->GetEnable() )
			{
				thisCheckCount++;
				bool isChanged = false;
				cam->check_NetUseable( &isChanged );

				if( isChanged )
				{
					m_handler->OnDeviceNotify( defDeviceNotify_StateChanged, (*it), NULL );
				}
			}
		}

		if( timeGetTime()-dwStart > 2000 )
		{
			m_last_checkNetUseable_camid = (*it)->getId();

			LOGMSG( "check_all_NetUseable long time=%ums, lastid=%d, count=%u, timeMax=%u", timeGetTime()-dwStart, m_last_checkNetUseable_camid, thisCheckCount, timeMax );
			return;
		}
	}

	m_last_checkNetUseable_camid = 0;
	m_last_checkNetUseable_time = timeGetTime();
	LOGMSG( "check_all_NetUseable all end, usetime=%ums, count=%u, timeMax=%u", timeGetTime()-dwStart, thisCheckCount, timeMax );
}

// 判断时间，生成校时队列
void GSIOTClient::check_all_devtime( const bool CheckNow )
{
	if( !IsRUNCODEEnable(defCodeIndex_SYS_CheckDevTimeCfg) )
	{
		return;
	}

	bool doCheck = CheckNow;

	static uint32_t s_last_check_check_all_devtime = timeGetTime();
	const uint32_t dwStart = timeGetTime();
	if( !doCheck )
	{
		if( dwStart-s_last_check_check_all_devtime > 30000 )
		{
			doCheck = true;
		}
	}

	if( !doCheck )
	{
		return;
	}

	s_last_check_check_all_devtime = dwStart;

	SYSTEMTIME st;
	memset( &st, 0, sizeof(st) );
	::GetLocalTime(&st);

	const int check_all_devtime_iday = RUNCODE_Get(defCodeIndex_SYS_CheckDevTimeSave,defRunCodeValIndex_1);

	if( !CheckNow && check_all_devtime_iday == st.wDay )
		return;

	const int checkRangeBegin = RUNCODE_Get(defCodeIndex_SYS_CheckDevTimeCfg,defRunCodeValIndex_2);
	const int checkRangeEnd = RUNCODE_Get(defCodeIndex_SYS_CheckDevTimeCfg,defRunCodeValIndex_3);

	// checkRangeBegin至checkRangeEnd之内执行校时
	if( !CheckNow && (st.wHour<checkRangeBegin || st.wHour>=checkRangeEnd) )
		return;
	
	LOGMSG( "check_all_devtime: hourrange(%d-%d)%s", checkRangeBegin, checkRangeEnd, CheckNow?" CheckNow!":"" );

	this->m_RunCodeMgr.SetCodeAndSaveDb( defCodeIndex_SYS_CheckDevTimeSave, st.wDay );

	PlayMgrCmd_SetDevtimeNow();
}

// 校时队列执行
void GSIOTClient::check_all_devtime_proc()
{
	std::set<int> NeedIDList; // 准备进行校时的队列

	{
		gloox::util::MutexGuard mutexguard( m_mutex_PlayMgr );
		NeedIDList.swap( m_check_all_devtime_NeedIDList );
	}

	if( NeedIDList.empty() )
	{
		return;
	}

	LOGMSG( "check_all_devtime_proc: start" );

	const uint32_t dwStart = timeGetTime();
	uint32_t thisCheckCount = 0;
	char buf[64] = {0};

	struGSTime newtime;
	g_struGSTime_GetCurTime( newtime );
	uint32_t newtime_lastget = timeGetTime();

	while( !NeedIDList.empty() )
	{
		std::set<int>::const_iterator itNeed = NeedIDList.begin();
		const int id = *itNeed;
		NeedIDList.erase(itNeed);
		thisCheckCount++;
		
		GSIOTDevice *pDev = this->ipcamClient->GetIOTDevice(id);
		if( pDev && pDev->GetEnable() )
		{
			IPCameraBase *cam = (IPCameraBase*)pDev->getControl();
			if(cam)
			{
				// 摄像机本身
				const std::string key = std::string(cam->GetIPAddress()) + itoa( cam->GetPort(), buf, 10 );

				if( !check_all_devtime_IsChecked(key) )
				{
					m_check_all_devtime_CheckedIPList.insert( key );
					g_reget_struGSTime_GetCurTime( newtime, newtime_lastget, timeGetTime() );

					cam->SetCamTime( newtime );
				}

				// 摄像机所处录像机及其他
				if( !cam->isGetSelf(true) )
				{
					const std::string strip = cam->ConnUse_ip(true);
					const uint32_t port = cam->ConnUse_port(true);
					const std::string key = strip + itoa( port, buf, 10 );

					if( !check_all_devtime_IsChecked(key) )
					{
						m_check_all_devtime_CheckedIPList.insert( key );
						g_reget_struGSTime_GetCurTime( newtime, newtime_lastget, timeGetTime() );

						switch( cam->GetCameraType() )
						{
						case CameraType_hik:
							HikCamera::SetCamTime_Spec( newtime, pDev->getName(), (char*)strip.c_str(), port, cam->ConnUse_username(true), cam->ConnUse_password(true) );
							break;

						default:
							break;
						}
					}
				}
			}
		}

		if( timeGetTime()-dwStart > 2000 )
		{
			LOGMSG( "check_all_devtime_proc: long time=%ums, count=%u", timeGetTime()-dwStart, thisCheckCount );
			return;
		}
	}

	LOGMSG( "check_all_devtime_proc: end all, usetime=%ums, count=%u\r\n", timeGetTime()-dwStart, thisCheckCount );
}

bool GSIOTClient::check_all_devtime_IsChecked( const std::string &key )
{
	std::set<std::string>::const_iterator itIP = m_check_all_devtime_CheckedIPList.find( key );

	return ( itIP != m_check_all_devtime_CheckedIPList.end() );
}

bool GSIOTClient::hasNetUseableFailed() const
{
	const std::list<GSIOTDevice*> &cameraList = this->ipcamClient->GetCameraManager()->GetCameraList();
	std::list<GSIOTDevice*>::const_iterator it = cameraList.begin();
	for( ; it!=cameraList.end(); ++it )
	{
		IPCameraBase *cam = (IPCameraBase*)(*it)->getControl();
		if(cam)
		{
			if( (*it)->GetEnable() )
			{
				if( defUseable_OK != cam->get_NetUseable() )
					return false;
			}
		}
	}

	return true;
}

// 系统整体综合所有布防相关设置条件，目前是否处于布防生效状态
GSAGCurState_ GSIOTClient::GetAlarmGuardCurState() const
{
	if( !GetAlarmGuardGlobalFlag() )
	{
		return GSAGCurState_UnArmed;
	}

	const GSAGCurState_ curevt = GetAllEventsState();

	if( GSAGCurState_UnArmed == curevt )
	{
		return GSAGCurState_UnArmed;
	}
	else if( g_IsValidCurTimeInAlarmGuardState() )
	{
		return curevt;
	}

	return GSAGCurState_WaitTimeArmed;
}

// 布防撤防总开关
int GSIOTClient::GetAlarmGuardGlobalFlag() const
{
	return RUNCODE_Get(defCodeIndex_SYS_AlarmGuardGlobalFlag);
}

void GSIOTClient::SetAlarmGuardGlobalFlag( int flag )
{
	this->m_RunCodeMgr.SetCodeAndSaveDb( defCodeIndex_SYS_AlarmGuardGlobalFlag, flag );

	const std::list<GSIOTDevice*> devices = ipcamClient->GetCameraManager()->GetCameraList();
	for( std::list<GSIOTDevice*>::const_iterator it=devices.begin(); it!=devices.end(); ++it )
	{
		GSIOTDevice* dev = (*it);
		if( IOT_DEVICE_Camera != dev->getType() )
			continue;

		IPCameraBase *cam = (IPCameraBase*)dev->getControl();
		if( cam )
		{
			cam->SetCurAlarmState( defAlarmState_UnInit );
		}
	}
}

// 获取设备信息简要
std::string GSIOTClient::GetSimpleInfo( const GSIOTDevice *const iotdevice )
{
	if( !iotdevice )
	{
		return std::string("");
	}

	std::string str;

	switch( iotdevice->getType() )
	{
	case IOT_DEVICE_Camera:
		{
			if( iotdevice->getControl() )
			{
				IPCameraBase *cam = (IPCameraBase*)iotdevice->getControl();
				char buf[64] = {0};

				str = cam->GetIPAddress();
				str += ":";
				str += itoa( cam->GetPort(), buf, 10 );
				str += ":";
				str += itoa( cam->GetChannel(), buf, 10 );

				if( IsRUNCODEEnable( defCodeIndex_SYS_AutoPublishEnable ) )
				{
					if( cam->GetAdvAttr().get_AdvAttr( defCamAdvAttr_AutoPublish ) )
					{
						str += ", 保持发布";
					}
				}

				if( cam->GetAdvAttr().get_AdvAttr( defCamAdvAttr_AutoConnect ) )
				{
					str += ", 自动连接";
				}

				if( cam->GetAdvAttr().get_AdvAttr( defCamAdvAttr_SupportAlarm ) )
				{
					str += ", 带告警";
				}

				if( defRecMod_NoRec == cam->GetRecCfg().getrec_mod() )
				{
					str += ", 无录像";
				}

				if( defAudioSource_Null != cam->GetAudioCfg().get_Audio_Source() )
				{
					str += ", ";
					str += CAudioCfg::getstr_AudioSource( cam->GetAudioCfg().get_Audio_Source() ).c_str();
				}

				if( 0 != cam->GetStreamfmt() )
				{
					str += ", 码流";
					str += itoa( cam->GetStreamfmt(), buf, 10 );
				}

				if( 2000 != cam->getBufferTime() )
				{
					str += ", 视频缓冲参数=";
					str += itoa( cam->getBufferTime(), buf, 10 );
				}
			}
		}
		break;

	case IOT_DEVICE_RS485:
		{
			RS485DevControl *ctl = (RS485DevControl*)iotdevice->getControl();

			if( ctl )
			{
				const defAddressQueue& que = ctl->GetAddressList();
				char buf[64] = {0};

				std::string strobjname;
				uint32_t enableObjCount = 0;
				defAddressQueue::const_iterator it = que.begin();
				defAddressQueue::const_iterator itEnd = que.end();
				for( ; it!=itEnd; ++it )
				{
					DeviceAddress *pCurAddr = *it;
					if( !pCurAddr->GetEnable() )
						continue;

					enableObjCount++;

					strobjname += pCurAddr->GetObjName();
					strobjname += ", ";

					if( strobjname.size() > 255 )
						break;
				}

				str = "485地址:";
				str += itoa( ctl->GetDeviceid(), buf, 10 );
				str += ", 子设备";
				str += itoa( enableObjCount, buf, 10 );
				str += "个; ";
				str += strobjname;
			}
		}
		break;

	case IOT_DEVICE_Remote:
		{
			RFRemoteControl *pctl = (RFRemoteControl*)iotdevice->getControl();
			if( pctl )
			{
				const defButtonQueue& que = pctl->GetButtonList();
				char buf[64] = {0};

				std::string strobjname;
				const uint32_t enableObjCount = pctl->GetEnableCount();
				defButtonQueue::const_iterator it = que.begin();
				defButtonQueue::const_iterator itEnd = que.end();
				for( ; it!=itEnd; ++it )
				{
					RemoteButton *pCurButton = *it;
					if( !pCurButton->GetEnable() )
						continue;
					
					strobjname += pCurButton->GetObjName();
					strobjname += ", ";

					if( strobjname.size() > 255 )
						break;
				}

				const std::string cfgdesc = pctl->GetCfgDesc();

				if( cfgdesc.empty() )
				{
					str += itoa( enableObjCount, buf, 10 );
					str += "个按钮; ";
				}
				else
				{
					str += cfgdesc;
					str += "; ";
					str += itoa( enableObjCount, buf, 10 );
					str += "个对象; ";
				}

				str += strobjname;
			}
		}
		break;

	case IOT_DEVICE_Trigger:
		return GetSimpleInfo_ForSupportAlarm( iotdevice );

	default:
		break;
	}

	return str;
}

// 告警触发类设备信息简要
std::string GSIOTClient::GetSimpleInfo_ForSupportAlarm( const GSIOTDevice *const iotdevice )
{
	if( !iotdevice )
	{
		return std::string("");
	}

	std::string str;

	switch( iotdevice->getType() )
	{
	case IOT_DEVICE_Camera:
	case IOT_DEVICE_Trigger:
		{
			int count_Force = 0;

			int count_SMS_Event = 0;
			int count_EMAIL_Event = 0;
			int count_NOTICE_Event = 0;
			int count_CONTROL_Event = 0;
			int count_Eventthing_Event = 0;
			int count_CALL_Event = 0;

			std::list<ControlEvent*> &eventList = this->GetEvent()->GetEvents();
			std::list<ControlEvent*>::const_iterator it = eventList.begin();
			for( ; it!=eventList.end(); ++it )
			{
				if( !(*it)->GetEnable() )
					continue;

				if( (*it)->GetDeviceType() == iotdevice->getType() && (*it)->GetDeviceID() == iotdevice->getId() )
				{
					if( (*it)->isForce() )
					{
						count_Force++;
					}

					switch( (*it)->GetType() )
					{
					case SMS_Event:			count_SMS_Event++;			break;
					case EMAIL_Event:		count_EMAIL_Event++;		break;
					case NOTICE_Event:		count_NOTICE_Event++;		break;
					case CONTROL_Event:		count_CONTROL_Event++;		break;
					case Eventthing_Event:	count_Eventthing_Event++;	break;
					case CALL_Event:		count_CALL_Event++;			break;
					}
				}
			}

			char buf[64] = {0};
			if( count_Force>0 )				{ str += itoa(count_Force,buf,10);			str += "条总是执行; "; }
			if( count_SMS_Event>0 )			{ str += itoa(count_SMS_Event,buf,10);			str += "条"; str += GSIOTEvent::GetEventTypeToString(SMS_Event);		str += ", "; }
			if( count_EMAIL_Event>0 )		{ str += itoa(count_EMAIL_Event,buf,10);		str += "条"; str += GSIOTEvent::GetEventTypeToString(EMAIL_Event);		str += ", "; }
			if( count_NOTICE_Event>0 )		{ str += itoa(count_NOTICE_Event,buf,10);		str += "条"; str += GSIOTEvent::GetEventTypeToString(NOTICE_Event);		str += ", "; }
			if( count_CONTROL_Event>0 )		{ str += itoa(count_CONTROL_Event,buf,10);		str += "条"; str += GSIOTEvent::GetEventTypeToString(CONTROL_Event);	str += ", "; }
			if( count_Eventthing_Event>0 )	{ str += itoa(count_Eventthing_Event,buf,10);	str += "条"; str += GSIOTEvent::GetEventTypeToString(Eventthing_Event);	str += ", "; }
			if( count_CALL_Event>0 )		{ str += itoa(count_CALL_Event,buf,10);			str += "条"; str += GSIOTEvent::GetEventTypeToString(CALL_Event);		str += ", "; }
		}
		break;

	default:
		break;
	}

	return str;
}

std::string GSIOTClient::getstr_ForRelation( const deflstRelationChild &ChildList )
{
	std::string str;

	for( deflstRelationChild::const_iterator it=ChildList.begin(); it!=ChildList.end(); ++it )
	{
		GSIOTDevice *dev = this->GetIOTDevice( it->child_dev_type, it->child_dev_id );
		if( !dev )
			continue;

		if( !str.empty() )
		{
			str += ", ";
		}
		
		str += dev->getName();
	}

	return str;
}

defGSReturn GSIOTClient::SendControl( const IOTDeviceType DevType, const GSIOTDevice *device, const GSIOTObjBase *obj, const uint32_t overtime, const uint32_t QueueOverTime, const uint32_t nextInterval, const bool isSync )
{
	if( device && IOT_DEVICE_Remote==DevType )
	{
		switch( device->getExType() )
		{
		case IOTDevice_AC_Ctl:
		{
			if( isSync )
			{
				const GSRemoteCtl_AC *acctl = (GSRemoteCtl_AC*)device->getControl();
				if( acctl && defFactory_ZK == acctl->get_factory() )
				{
					if( m_IGSMessageHandler )
					{
						return m_IGSMessageHandler->OnControlOperate( defCtrlOprt_SendControl, device->getExType(), device, obj );
					}

					return defGSReturn_Err;
				}

				return defGSReturn_UnSupport;
			}

			GSIOTDevice *sendDev = device->clone( false );
			RFRemoteControl *sendctl = (RFRemoteControl*)sendDev->getControl();
			if( obj ) sendctl->ButtonQueueChangeToOne( obj->GetId() );

			RFRemoteControl *ctl = (RFRemoteControl*)device->getControl();
			sendctl->SetCmd( ctl->GetCmd() );

			PushControlMesssageQueue( new ControlMessage( JID(), "", sendDev, obj?sendctl->GetButton( obj->GetId() ):NULL, QueueOverTime ) );
			return defGSReturn_SuccExecuted;
		}
		break;
		
		case IOTDevice_Combo_Ctl:
		{
			struGSTime curdt;
			g_struGSTime_GetCurTime( curdt );

			this->DoAlarmDevice( device, true, 1, true, device->GetStrAlmBody( true, curdt ), device->GetStrAlmSubject( true ) );
			break;
		}

		default:
			break;
		}
	}

	return this->deviceClient->SendControl( DevType, device, obj, overtime, QueueOverTime, nextInterval );
}

bool GSIOTClient::ACProcOne()
{
	ControlMessage *pCtlMsg = PopControlMesssageQueue( NULL, NULL, NULL, IOT_DEVICE_Remote, IOTDevice_AC_Ctl );
	if( pCtlMsg )
	{
		defGSReturn ret = defGSReturn_Err;
		GSRemoteCtl_AC *acctl = (GSRemoteCtl_AC*)pCtlMsg->GetDevice()->getControl();
		if( acctl && defFactory_ZK == acctl->get_factory() )
		{
			if( m_IGSMessageHandler )
			{
				ret = m_IGSMessageHandler->OnControlOperate( defCtrlOprt_SendControl, pCtlMsg->GetDevice()->getExType(), pCtlMsg->GetDevice(), pCtlMsg->GetObj() );

				//传送给UI
				if( m_handler && macGSSucceeded(ret) )
				{
					if( defCmd_Get_ObjState == acctl->GetCmd() )
					{
						m_handler->OnDeviceData( acctl->GetLinkID(), pCtlMsg->GetDevice(), acctl, pCtlMsg->GetObj()?pCtlMsg->GetObj():acctl->GetFristButton() );
					}
				}
			}
		}
		else
		{
			ret = defGSReturn_UnSupport;
		}

		if( pCtlMsg->GetJid() )
		{
			IQ re( IQ::Result, pCtlMsg->GetJid(), pCtlMsg->GetId() );
			re.addExtension( new GSIOTControl( pCtlMsg->GetDevice()->clone(), defUserAuth_RW, false ) );
			XmppClientSend( re, "ACProcOne Send" );
		}

		delete pCtlMsg;

		return true;
	}

	return false;
}

bool GSIOTClient::ACProc()
{
	const bool working = ACProcOne();

	if( timeGetTime()-m_lastcheck_AC < 30*1000 )
		return working;

	const uint32_t dwStart = timeGetTime();
	LOGMSG( "ACProc check...\r\n" );

	int checked = 0;

	std::list<GSIOTDevice*> lstDev = this->GetDeviceConnection()->GetDeviceManager()->GetDeviceList();
	for( std::list<GSIOTDevice*>::const_iterator it=lstDev.begin(); it!=lstDev.end(); ++it )
	{
		if( !is_running() )
			return true;

		const GSIOTDevice* dev = (*it);

		if( !dev->GetEnable() )
			continue;

		if( !dev->getControl() )
			continue;

		const uint32_t dwStep = timeGetTime();

		if( IOT_DEVICE_Remote == dev->getType()
			&& IOTDevice_AC_Ctl == dev->getControl()->GetExType()
			)
		{
			GSRemoteCtl_AC *acctl = (GSRemoteCtl_AC*)dev->getControl();
			if( defFactory_ZK == acctl->get_factory() )
			{
				if( m_IGSMessageHandler )
				{
					const defGSReturn retCheck = m_IGSMessageHandler->OnControlOperate( defCtrlOprt_CheckHeartbeat, dev->getExType(), dev, NULL );
					if( macGSSucceeded(retCheck) )
						checked++;
				}
			}
		}

		const uint32_t dwStepUse = timeGetTime()-dwStep;
		if( dwStepUse > 1000 )
		{
			LOGMSG( "ACProc check stepuse=%dms, devid=%d\r\n", dwStepUse, dev->getId() );
		}

		ACProcOne();
	}

	m_lastcheck_AC = timeGetTime();
	LOGMSG( "ACProc checked=%d, usetime=%dms\r\n", checked, timeGetTime()-dwStart );

	return true;
}

void GSIOTClient::ACProc_ThreadCreate()
{
	m_isACProcThreadExit = false;

	LOGMSGEX( defLOGNAME, defLOG_SYS, "ACProcThread Running..." );
	HANDLE   hth1;
	unsigned  uiThread1ID;
	hth1 = (HANDLE)_beginthreadex( NULL, 0, ACProcThread, this, 0, &uiThread1ID );
	CloseHandle(hth1);
}
