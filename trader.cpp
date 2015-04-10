#include "tradeapi.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcUserApiDataType.h"
#include "ThostFtdcUserApiStruct.h"
#include "config.h"
#include "log4z.h"
#include <iostream>
#include <stdio.h>
#include <signal.h>
using namespace zsummer::log4z;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

TThostFtdcBrokerIDType g_chBrokerID;
TThostFtdcUserIDType g_chUserID;
string g_strFlowPath;
string g_strBrokerID;
string g_strUserID;
string g_strFrontURL;
string g_strPassword;
string g_strLogPath;

class CSimpleHandler : public CThostFtdcTraderSpi
{
public:
    // constructor,which need a valid pointer to a CThostFtdcMduserApi instance
    CSimpleHandler(CThostFtdcTraderApi *pUserApi) :m_pUserApi(pUserApi) {}
    ~CSimpleHandler() {}
    //After making a succeed connection with the CTP server, the
    //client should send the login request to the CTP server.
    virtual void OnFrontConnected()
    {
        CThostFtdcReqUserLoginField reqUserLogin;
        // get BrokerID
        cout<<"OnFrontConnected"<<endl;
        LOGFMTI("BrokerID:%s", g_strBrokerID);
        //scanf("%s", &g_chBrokerID);
        strcpy(reqUserLogin.BrokerID, g_strBrokerID.c_str());
        // get user id
        LOGFMTI("userid:%s", g_chUserID);
        //scanf("%s", &g_chUserID);
        strcpy(reqUserLogin.UserID, g_strUserID.c_str());
        // get password
        LOGFMTI("password:%s", g_strPassword);
        //scanf("%s", &reqUserLogin.Password);
        // send the login request
        strcpy(reqUserLogin.Password, g_strPassword.c_str());
        m_pUserApi->ReqUserLogin(&reqUserLogin, 0);
    }
    //When the connection between client and the CTP server disconnected,the follwing function will be called.
    virtual void OnFrontDisconnected(int nReason)
    {
        // Inthis case, API willreconnect,the client application can ignore this.
        LOGFMTI("OnFrontDisconnected.");
    }
    // After receiving the login request from the client,the CTP server will send the following response to notify the client whether the login success or not.
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField
                                *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID,
                                bool bIsLast)
    {
        LOGFMTI("ErrorCode=[%d], ErrorMsg=[%s]", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        LOGFMTI("RequestID=[%d], Chain=[%d]", nRequestID, bIsLast);
        if (pRspInfo->ErrorID != 0) {
            // in case any login failure, the client should handle this error.
            LOGFMTI("Failed to login, errorcode=%d errormsg=%s requestid=%d chain=%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast);
            exit(-1);
        }
        // login success, then send order insertion request.
        CThostFtdcInputOrderField ord;
        memset(&ord, 0, sizeof(ord));
        //broker id
        strcpy(ord.BrokerID, g_chBrokerID);
        //investor ID
        strcpy(ord.InvestorID, "12345");
        // instrument ID
        strcpy(ord.InstrumentID, "cn0601");
        ///order reference
        strcpy(ord.OrderRef, "000000000001");
        // user id
        strcpy(ord.UserID, g_strUserID.c_str());
        // order price type
        ord.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        // direction
        ord.Direction = THOST_FTDC_D_Buy;
        // combination order’s offset flag
        strcpy(ord.CombOffsetFlag, "0");
        // combination or hedge flag
        strcpy(ord.CombHedgeFlag, "1");
        // price
        ord.LimitPrice = 50000;
        // volume
        ord.VolumeTotalOriginal = 10;
        // valid date
        ord.TimeCondition = THOST_FTDC_TC_GFD;
        // GTD DATE
        strcpy(ord.GTDDate, "");
        // volume condition
        ord.VolumeCondition = THOST_FTDC_VC_AV;
        // min volume
        ord.MinVolume = 0;
        // trigger condition
        ord.ContingentCondition = THOST_FTDC_CC_Immediately;
        // stop price
        ord.StopPrice = 0;
        // force close reason
        ord.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
        // auto suspend flag
        ord.IsAutoSuspend = 0;
        m_pUserApi->ReqOrderInsert(&ord, 1);
    }
    // order insertion response
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        LOGFMTI("ErrorCode=[%d], ErrorMsg=[%s]",
               pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        // inform the main thread order insertion is over
        pthread_cond_signal(&cond);
    };
    ///order insertion return
    virtual void OnRtnOrder(CThostFtdcOrderField *pOrder)
    {
        LOGFMTI("OnRtnOrder:");
        LOGFMTI("OrderSysID=[%s]", pOrder->OrderSysID);
    }
    // the error notification caused by client request
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo,int nRequestID, bool bIsLast) {
        LOGFMTI("OnRspError:");
        LOGFMTI("ErrorCode=[%d], ErrorMsg=[%s]",
               pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        LOGFMTI("RequestID=[%d], Chain=[%d]", nRequestID,
               bIsLast);
        // the client should handle the error
        //{error handle code}
    }
private:
    // a pointer of CThostFtdcMduserApi instance
    CThostFtdcTraderApi *m_pUserApi;
};
void sighandler(int sig)
{
    pthread_cond_signal(&cond);
}
int main(int argc, char *argv[]) {

    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    ILog4zManager::GetInstance()->Start();
    LOGI("Trader Application Start...");
    LOGI("Read params from config file");
    Config cfg("trader.config",0);
    g_strFlowPath = cfg.pString("TRADER_FLOW_PATH");
    g_strBrokerID = cfg.pString("BROKER_ID");
    g_strUserID = cfg.pString("USER_ID");
    g_strPassword = cfg.pString("PASSWORD");
    g_strLogPath = cfg.pString("LOG_PATH");
    g_strFrontURL = cfg.pString("TRADER_URL");

    //create a CThostFtdcTraderApi instance
    CThostFtdcTraderApi *pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(g_strFlowPath.c_str());
    // create an event handler instance
    CSimpleHandler sh(pUserApi);
    // register an event handler instance
    try{
        pUserApi->RegisterSpi(&sh);
        // subscribe private topic
        pUserApi->SubscribePrivateTopic(THOST_TERT_RESUME);
        // subscribe public topic
        pUserApi->SubscribePublicTopic(THOST_TERT_RESUME);
        char ch_URL[100];
        strcpy(ch_URL, g_strFrontURL.c_str());
        LOGFMTI("Register the CTP front address and port %s" ch_URL);
        pUserApi->RegisterFront(ch_URL);
        LOGI("Make the connection between client and CTP server");
        pUserApi->Init();
        LOGI("Waiting for the order insertion.");
        pthread_cond_wait(&cond, &mutex);
    }
    catch(exception &e){
        LOGE("exception caught:"<< e.what());
    }
    LOGI("Release the API & SPI instance");
    if (pUserApi){
        pUserApi->RegisterSpi(NULL);
        pUserApi->Release();
        pUserApi = NULL;
    }
    if (&sh) {
    //    delete &sh;
        sh = NULL;
    }
    LOGI("Trader Application quit!");
    return 0;
}