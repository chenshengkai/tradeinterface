// tradeapitest.cpp :
#include "md.h"
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

// the flag whether the quotation data received or not.
// Create a manual reset event with no signal
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
string g_strMktData;

void concatMarketData(string &strMktData, CThostFtdcDepthMarketDataField *p){
    char buffer[1000];
    //sprintf(buffer, "%s", p->TradingDay);
    sprintf(buffer, "%s,%s,%s,%s,%f,%f,%f,%f,%f,%f,%f,%i,%f,%f,%f,%f,%f,%f,%f,%f,%s,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%i,%f,%s",
        p->TradingDay, p->InstrumentID, p->ExchangeID, p->ExchangeInstID,
        p->LastPrice, p->PreSettlementPrice, p->PreClosePrice, p->PreOpenInterest, p->OpenPrice, p->HighestPrice, 
        p->LowestPrice, p->Volume, p->Turnover, p->OpenInterest, p->ClosePrice, p->SettlementPrice, p->UpperLimitPrice,
        p->LowerLimitPrice, p->PreDelta, p->CurrDelta, p->UpdateTime, p->UpdateMillisec, p->BidPrice1, p->BidVolume1,
        p->AskPrice1, p->AskVolume1, p->BidPrice2, p->BidVolume2, p->AskPrice2, p->AskVolume2, p->BidPrice3, p->BidVolume3, 
        p->AskPrice3, p->AskVolume3,
        p->BidPrice4, p->BidVolume4, p->AskPrice4, p->AskVolume4, p->BidPrice5, p->BidVolume5, p->AskPrice5, p->AskVolume5, 
        p->AveragePrice, p->ActionDay);
    strMktData.assign(buffer);
} 

class CSimpleHandler : public CThostFtdcMdSpi
{
public:
    // constructor,which need a valid pointer of a CThostFtdcMdApi instance
    CSimpleHandler(CThostFtdcMdApi *pUserApi) :m_pUserApi(pUserApi) {
    }
    ~CSimpleHandler() {}
    // when the connection between client and CTP server is created successfully, the client would send the login request to the CTP server.
    virtual void OnFrontConnected() {
        LOGI("Front connecting...");
        CThostFtdcReqUserLoginField reqUserLogin;
//        scanf("%s", &g_chBrokerID);
        strcpy(reqUserLogin.BrokerID, g_strBrokerID.c_str());
        LOGI("BrokerID is : "<<reqUserLogin.BrokerID);
        strcpy(reqUserLogin.UserID, g_strUserID.c_str());
        LOGI("UserID is : " <<reqUserLogin.UserID);
        strcpy(reqUserLogin.Password, g_strPassword.c_str());
        LOGI("Password is : " << reqUserLogin.Password);
        m_pUserApi->ReqUserLogin(&reqUserLogin, 0);
    }
    // when client and CTP server disconnected,the follwing function will be called.
    virtual void OnFrontDisconnected(int nReason) {
        
        // inhtis case, API will reconnect,the client application canignore this.
        LOGI("OnFrontDisconnected.");
    }
    // after receiving the login request from the client,the CTP server will send the following response to notify the client whether the login success or not.
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        LOGI("User Login start ...");
        LOGFMTI("ErrorCode=[%d], ErrorMsg=[%s]", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        LOGFMTI("RequestID=[%d], Chain=[%d]", nRequestID, bIsLast);
        if (pRspInfo->ErrorID != 0) {
        // login failure, the client should handle this error.
            LOGFMTI("Failed to login, errorcode=%d errormsg=%s requestid=%d chain=%d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast);
            exit(-1); 
        }
        LOGI("User Login success, then subscribe the quotation information");
        char * Instrumnet[]={"IF1407","IF1408"};
        m_pUserApi->SubscribeMarketData (Instrumnet,2);
        //or unsubscribe the quotation
        //m_pUserApi->UnSubscribeMarketData (Instrumnet,2); 
    }
        // quotation return
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
    {
        //output the order insert result
        concatMarketData(g_strMktData, pDepthMarketData);
        LOGFMTI(g_strMktData.c_str());
        // set the flag when the quotation data received.
        //pthread_cond_signal(&cond);
    };
    // the error notification caused by client request
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        LOGI("OnRspError:");
        LOGFMTI("ErrorCode=[%d], ErrorMsg=[%s]", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        LOGFMTI("RequestID=[%d], Chain=[%d]", nRequestID, bIsLast);
        // the client should handle the error
        //{error handle code}
        pthread_cond_signal(&cond);
    }
private:
    // a pointer to CThostFtdcMdApi instance
    CThostFtdcMdApi *m_pUserApi;
};

void sighandler(int sig){
    pthread_cond_signal(&cond);
}

int main(int argc, char *argv[])
{
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    ILog4zManager::GetInstance()->Start();
    LOGI("MD Application Start...");
    LOGI("Read params from config file");
    Config cfg("trader.config",0);
    g_strFlowPath = cfg.pString("MD_FLOW_PATH");
    g_strBrokerID = cfg.pString("BROKER_ID");
    g_strUserID = cfg.pString("USER_ID");
    g_strPassword = cfg.pString("PASSWORD");
    g_strLogPath = cfg.pString("LOG_PATH");
    g_strFrontURL = cfg.pString("MD_URL");

    // create a CThostFtdcMdApi instance
    CThostFtdcMdApi *pUserApi = CThostFtdcMdApi::CreateFtdcMdApi(g_strFlowPath.c_str());
    // create an event handler instance
    CSimpleHandler sh(pUserApi);
    try{
        LOGI("register an event handler instance");
        pUserApi->RegisterSpi(&sh);
        LOGI("register the CTP front address and port");
        char ch_URL[100];   
        strcpy(ch_URL, g_strFrontURL.c_str());
        pUserApi->RegisterFront(ch_URL);
        LOGFMTI("start to connect CTP server : %s", ch_URL);
        pUserApi->Init();
        LOGI("waiting for the quotation data");
        pthread_cond_wait(&cond, &mutex);
        LOGI("release API & SPI instance");
    }
    catch(exception& e)
    {
        LOGE("exception caught:"<< e.what());
    }
    if (pUserApi){
        pUserApi->RegisterSpi(NULL);
        pUserApi->Release();
        pUserApi = NULL;
    }
    if (&sh) {
//        delete &sh;
        sh = NULL;
    }
    LOGI("MD Application quit!");
    return 0;
}