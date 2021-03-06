/*
 * 消息处理
 * Copyright (C) 2019 String.Empty
 */
#pragma once
#ifndef DICE_EVENT
#define DICE_EVENT
#include <map>
#include <set>
#include <sstream>
#include "CQAPI_EX.h"
#include "MsgMonitor.h"
#include "DiceMsgSend.h"
#include "GlobalVar.h"
#include "MsgFormat.h"
#include "initList.h"
#include "NameStorage.h"
//打包待处理消息
using namespace std;
using namespace CQ;



extern map<long long, int> DefaultDice;
//默认COC检定房规
extern map<chatType, int> mDefaultCOC;
extern map<long long, string> WelcomeMsg;
extern map<long long, string> DefaultRule;
extern set<long long> DisabledJRRPGroup;
extern set<long long> DisabledJRRPDiscuss;
extern set<long long> DisabledMEGroup;
extern set<long long> DisabledMEDiscuss;
extern set<long long> DisabledHELPGroup;
extern set<long long> DisabledHELPDiscuss;
extern set<long long> DisabledOBGroup;
extern set<long long> DisabledOBDiscuss;
extern unique_ptr<NameStorage> Name;
extern unique_ptr<Initlist> ilInitList;
struct SourceType
{
	SourceType(long long a, int b, long long c) : QQ(a), Type(b), GrouporDiscussID(c)
	{
	}
	long long QQ = 0;
	int Type = 0;
	long long GrouporDiscussID = 0;

	bool operator<(SourceType b) const
	{
		return this->QQ < b.QQ;
	}
};

using PropType = map<string, int>;
extern map<SourceType, PropType> CharacterProp;
extern multimap<long long, long long> ObserveGroup;
extern multimap<long long, long long> ObserveDiscuss;
class FromMsg {
public:
	std::string strMsg;
	string strLowerMessage;
	long long fromID = 0;
	CQ::msgtype fromType = CQ::Private;
	long long fromQQ = 0;
	long long fromGroup = 0;
	chatType fromChat;
	time_t fromTime = time(NULL);
	string strReply;
	FromMsg(std::string message, long long fromNum) :strMsg(message), fromQQ(fromNum), fromID(fromNum) {
		fromChat = { fromID,Private };
		mLastMsgList[fromChat] = fromTime;
	}

	FromMsg(std::string message, long long fromGroup, CQ::msgtype msgType, long long fromNum) :strMsg(message), fromQQ(fromNum), fromType(msgType), fromID(fromGroup), fromGroup(fromGroup), fromChat({ fromGroup,fromType }) {
		mLastMsgList[fromChat] = fromTime;
	}

	void reply(std::string strReply) {
		AddMsgToQueue(strReply, fromID, fromType);
	}
	void reply() {
		AddMsgToQueue(strReply, fromID, fromType);
	}
	//通知管理员
	void AdminNotify(std::string strMsg) {
		reply(strMsg);
		string strName = getName(fromQQ);
		for (auto it : AdminQQ) {
			if (MonitorList.count({ it,Private }) && fromQQ != it) AddMsgToQueue(strName + strMsg, it);
		}
	}
	//转发消息
	void FwdMsg(string message) {
		if (mFwdList.count(fromChat)&&!isLinkOrder) {
			auto range = mFwdList.equal_range(fromChat);
			string strFwd;
			if (fromType == Group)strFwd += "[群:"+to_string(fromGroup)+"]";
			if (fromType == Discuss)strFwd += "[讨论组:" + to_string(fromGroup) + "]";
			strFwd += getName(fromQQ,fromGroup) + "(" + to_string(fromQQ) + "):";
			if (masterQQ == fromQQ)strFwd.clear();
			strFwd += message;
			for (auto it = range.first; it != range.second; it++) {
				AddMsgToQueue(strFwd, it->second.first, it->second.second);
			}
		}
	}
	int AdminEvent(string strOption) {
		if (strOption == "state") {
			strReply = GlobalMsg["strSelfName"] + "的当前情况" + "\n"
				+ "Master：" + printQQ(masterQQ) + "\n"
				+ (ClockToWork.first < 24 ? "定时开启" + printClock(ClockToWork) + "\n" : "")
				+ (ClockOffWork.first < 24 ? "定时关闭" + printClock(ClockOffWork) + "\n" : "")
				+ (boolConsole["Private"] ? "私用模式" : "公用模式") + "\n"
				+ (boolConsole["LeaveDiscuss"] ? "禁用讨论组" : "启用讨论组") + "\n"
				+ "全局开关：" + (boolConsole["DisabledGlobal"] ? "禁用" : "启用") + "\n"
				+ "全局.me开关：" + (boolConsole["DisabledMe"] ? "禁用" : "启用") + "\n"
				+ "全局.jrrp开关：" + (boolConsole["DisabledJrrp"] ? "禁用" : "启用");
			if (isAdmin) strReply += "\n所在群聊数：" + to_string(getGroupList().size()) + "\n"
				+ "黑名单用户数：" + to_string(BlackQQ.size()) + "\n"
				+ "黑名单群数：" + to_string(BlackGroup.size()) + "\n"
				+ "白名单用户数：" + to_string(WhiteQQ.size()) + "\n"
				+ "白名单群数：" + to_string(WhiteGroup.size());
			reply();
			return 1;
		}
		if (!isAdmin) {
			reply(GlobalMsg["strNotAdmin"]);
			return -1;
		}
		if (boolConsole.count(strOption)) {
			string strBool = readDigit();
			if (strBool.empty())boolConsole[strOption] ? reply(GlobalMsg["strSelfName"] + "该项当前正开启") : reply(GlobalMsg["strSelfName"] + "该项当前正关闭");
			else {
				bool isOn = stoi(strBool);
				boolConsole[strOption] = isOn;
				isOn ? AdminNotify("已开启" + GlobalMsg["strSelfName"] + "的" + strOption) : AdminNotify("已关闭" + GlobalMsg["strSelfName"] + "的" + strOption);
			}
			return 1;
		}
		else if (strOption == "delete") {
			AdminNotify("已经放弃管理员权限√");
			MonitorList.erase({ fromQQ,Private });
			AdminQQ.erase(fromQQ);
			return 1;
		}
		else if (strOption == "on") {
			if (boolConsole["DisabledGlobal"]) {
				boolConsole["DisabledGlobal"] = false;
				AdminNotify("已全局开启" + GlobalMsg["strSelfName"]);
			}
			else {
				reply(GlobalMsg["strSelfName"] + "不在静默中！");
			}
			return 1;
		}
		else if (strOption == "off") {
			if (boolConsole["DisabledGlobal"]) {
				reply(GlobalMsg["strSelfName"] + "已经静默！");
			}
			else {
				boolConsole["DisabledGlobal"] = true;
				AdminNotify("已全局关闭" + GlobalMsg["strSelfName"]);
			}
			return 1;
		}
		else if (strOption == "dicelist")
		{
			getDiceList();
			strReply = "当前骰娘列表：";
			for (auto it : mDiceList) {
				strReply += "\n" + printQQ(it.first);
			}
			reply();
			return 1;
		}
		else if (strOption == "meon") {
			if (boolConsole["DisabledMe"]) {
				boolConsole["DisabledMe"] = false;
				AdminNotify("已令" + GlobalMsg["strSelfName"] + "全局启用.me√");
			}
			else {
				reply(GlobalMsg["strSelfName"] + "已启用.me！");
			}
			return 1;
		}
		else if (strOption == "meoff") {
			if (boolConsole["DisabledMe"]) {
				reply(GlobalMsg["strSelfName"] + "已禁用.me！");
			}
			else {
				boolConsole["DisabledMe"] = true;
				AdminNotify("已令" + GlobalMsg["strSelfName"] + "全局禁用.me√");
			}
			return 1;
		}
		else if (strOption == "jrrpon") {
			if (boolConsole["DisabledMe"]) {
				boolConsole["DisabledMe"] = false;
				AdminNotify("已令" + GlobalMsg["strSelfName"] + "全局启用.jrrp√");
			}
			else {
				reply(GlobalMsg["strSelfName"] + "已启用.jrrp！");
			}
			return 1;
		}
		else if (strOption == "jrrpoff") {
			if (boolConsole["DisabledMe"]) {
				reply(GlobalMsg["strSelfName"] + "已禁用.jrrp！");
			}
			else {
				boolConsole["DisabledMe"] = true;
				AdminNotify("已令" + GlobalMsg["strSelfName"] + "全局禁用.jrrp√");
			}
			return 1;
		}
		else if (strOption == "discusson") {
			if (boolConsole["LeaveDiscuss"]) {
				boolConsole["LeaveDiscuss"] = false;
				AdminNotify("已关闭讨论组自动退出√");
			}
			else {
				reply(GlobalMsg["strSelfName"] + "已关闭讨论组自动退出！");
			}
			return 1;
		}
		else if (strOption == "discussoff") {
			if (boolConsole["LeaveDiscuss"]) {
				reply(GlobalMsg["strSelfName"] + "已开启讨论组自动退出！");
			}
			else {
				boolConsole["LeaveDiscuss"] = true;
				AdminNotify("已开启讨论组自动退出√");
			}
			return 1;
		}
		else if (strOption == "only") {
		if (boolConsole["Private"]) {
			reply(GlobalMsg["strSelfName"] + "已成为私用骰娘！");
		}
		else {
			boolConsole["Private"] = true;
			AdminNotify("已将" + GlobalMsg["strSelfName"] + "变为私用√");
		}
		return 1;
			}
		else if (strOption == "public") {
		if (boolConsole["Private"]) {
			boolConsole["Private"] = false;
			AdminNotify("已将" + GlobalMsg["strSelfName"] + "变为公用√");
		}
		else {
			reply(GlobalMsg["strSelfName"] + "已成为公用骰娘！");
		}
		return 1;
			}
		else if (strOption == "clock") {
		string strReply = "本地时间" + to_string(stNow.wHour) + ":" + to_string(stNow.wMinute);
		reply(strReply);
		return 1;
		}
		else if (strOption == "clockon") {
		string strHour = readDigit();
		if (strHour.empty() || stoi(strHour) > 23) {
			ClockToWork = { 24,00 };
			AdminNotify("已清除定时启用");
			return 1;
		}
		int intHour = stoi(strHour);
		string strMinute = readDigit();
		if (strMinute.empty()) strMinute = "00";
		int intMinute = stoi(strMinute);
		ClockToWork = { intHour,intMinute };
		AdminNotify("已设置定时启用时间" + printClock(ClockToWork));
		return 1;
			}
		else if (strOption == "clockoff") {
		string strHour = readDigit();
		if (strHour.empty() || stoi(strHour) > 23) {
			ClockOffWork = { 24,00 };
			AdminNotify("已清除定时关闭");
			return 1;
		}
		int intHour = stoi(strHour);
		string strMinute = readDigit();
		if (strMinute.empty()) strMinute = "00";
		int intMinute = stoi(strMinute);
		ClockOffWork = { intHour,intMinute };
		AdminNotify("已设置定时关闭时间" + printClock(ClockOffWork));
		return 1;
			}
		else if (strOption == "monitor") {
		bool boolErase = false;
		readSkipSpace();
		if (strMsg[intMsgCnt] == '-') {
			boolErase = true;
			intMsgCnt++;
		}
		if (strMsg[intMsgCnt] == '+') { intMsgCnt++; }
		chatType cTarget;
		if (readChat(cTarget)) {
			strReply = "当前监视窗口" + to_string(MonitorList.size()) + "个：";
			for (auto it : MonitorList) {
				strReply += "\n" + printChat(it);
			}
			reply();
			return 1;
		}
		if (boolErase) {
			if (MonitorList.count(cTarget)) {
				MonitorList.erase(cTarget);
				AdminNotify("已移除监视窗口" + printChat(cTarget) + "√");
			}
			else {
				reply("该窗口不存在于监视列表！");
			}
		}
		else {
			if (MonitorList.count(cTarget)) {
				reply("该窗口已存在于监视列表！");
			}
			else {
				MonitorList.insert(cTarget);
				AdminNotify("已添加监视窗口" + printChat(cTarget) + "√");
			}
		}
		return 1;
		}
		else {
			bool boolErase = false;
			string strReason = readPara();
			if (strMsg[intMsgCnt] == '-') {
				boolErase = true;
				intMsgCnt++;
			}
			if (strMsg[intMsgCnt] == '+') {intMsgCnt++;}
			long long llTargetID = readID();
			if (strOption == "dismiss") {
				WhiteGroup.erase(llTargetID);
				if (getGroupList().count(llTargetID)) {
					setGroupLeave(llTargetID);
					mLastMsgList.erase({ llTargetID ,Group });
					AdminNotify("已令" + GlobalMsg["strSelfName"] + "退出群" + to_string(llTargetID) + "√");
				}
				else if (llTargetID > 1000000000 && mLastMsgList.count({ llTargetID ,Discuss })) {
					setDiscussLeave(llTargetID);
					mLastMsgList.erase({ llTargetID ,Discuss });
					AdminNotify("已令" + GlobalMsg["strSelfName"] + "退出讨论组" + to_string(llTargetID) + "√");
				}
				else {
					reply(GlobalMsg["strGroupGetErr"]);
				}
				return 1;
			}
			else if (strOption == "boton") {
				if (getGroupList().count(llTargetID)) {
					if (DisabledGroup.count(llTargetID)) {
						DisabledGroup.erase(llTargetID);
						AdminNotify("已令" + GlobalMsg["strSelfName"] + "在" + printGroup(llTargetID) + "起用√");
					}
					else reply(GlobalMsg["strSelfName"] + "已在该群起用!");
				}
				else {
					reply(GlobalMsg["strGroupGetErr"]);
				}
			}
			else if (strOption == "botoff") {
				if (getGroupList().count(llTargetID)) {
					if (!DisabledGroup.count(llTargetID)) {
						DisabledGroup.insert(llTargetID);
						AdminNotify("已令" + GlobalMsg["strSelfName"] + "在" + printGroup(llTargetID) + "静默√");
					}
					else reply(GlobalMsg["strSelfName"] + "已在该群静默!");
				}
				else {
					reply(GlobalMsg["strGroupGetErr"]);
				}
				return 1;
			}
			else if (strOption == "whitegroup") {
				if (llTargetID == 0) {
					strReply = "当前白名单群列表：";
					for (auto each : WhiteGroup) {
						strReply += "\n" + printGroup(each);
					}
					reply();
					return 1;
				}
				do {
					if (boolErase) {
						if (WhiteGroup.count(llTargetID)) {
							WhiteGroup.erase(llTargetID);
							AdminNotify("已将" + printGroup(llTargetID) + "移出" + GlobalMsg["strSelfName"] + "的白名单√");
						}
						else {
							reply(printGroup(llTargetID) + "并不在" + GlobalMsg["strSelfName"] + "的白名单！");
						}
					}
					else {
						if (WhiteGroup.count(llTargetID)) {
							reply(printGroup(llTargetID) + "已加入" + GlobalMsg["strSelfName"] + "的白名单!");
						}
						else {
							WhiteGroup.insert(llTargetID);
							AdminNotify("已将" + printGroup(llTargetID) + "加入" + GlobalMsg["strSelfName"] + "的白名单√");
						}
					}
				} while (llTargetID = readID());
				return 1;
			}
			else if (strOption == "blackgroup") {
				if (llTargetID == 0) {
					strReply = "当前黑名单群列表：";
					for (auto each : BlackGroup) {
						strReply += "\n" + to_string(each);
					}
					reply();
					return 1;
				}
				BlackMark mark;
				mark.llMap = { {"DiceMaid",DiceMaid},{"masterQQ",masterQQ} };
				mark.strMap = { {"type","other"},{"time",printSTime(stNow)} };
				if (!strReason.empty())mark.set("note", strReason);
				do{
					if (boolErase) {
						if (BlackGroup.count(llTargetID)) {
							rmBlackGroup(llTargetID, fromQQ);
						}
						else {
							reply(printGroup(llTargetID) + "并不在" + GlobalMsg["strSelfName"] + "的黑名单！");
						}
					}
					else {
						if (BlackGroup.count(llTargetID)) {
							reply(printGroup(llTargetID) + "已加入" + GlobalMsg["strSelfName"] + "的黑名单!");
						}
						else {
							mark.set("fromGroup", llTargetID);
							if(addBlackGroup(mark))AdminNotify("已将" + printGroup(llTargetID) + "加入" + GlobalMsg["strSelfName"] + "的黑名单√");;
						}
					}
				} while (llTargetID = readID());
				return 1;
			}
			else if (strOption == "whiteqq") {
				if (llTargetID == 0) {
					strReply = "当前白名单用户列表：";
					for (auto each : WhiteQQ) {
						strReply += "\n" + printQQ(each);
					}
					reply();
					return 1;
				}
				do{
					if (boolErase) {
						if (WhiteQQ.count(llTargetID)) {
							WhiteQQ.erase(llTargetID);
							AdminNotify("已将" + printQQ(llTargetID) + "移出" + GlobalMsg["strSelfName"] + "的白名单√");
						}
						else {
							reply(printQQ(llTargetID) + "并不在" + GlobalMsg["strSelfName"] + "的白名单！");
						}
					}
					else {
						if (WhiteQQ.count(llTargetID)) {
							reply(printQQ(llTargetID) + "已加入" + GlobalMsg["strSelfName"] + "的白名单!");
						}
						else {
							WhiteQQ.insert(llTargetID);
							AdminNotify("已将" + printQQ(llTargetID) + "加入" + GlobalMsg["strSelfName"] + "的白名单√");
							AddMsgToQueue(GlobalMsg["strWhiteQQAddNotice"], llTargetID);
						}
					}
				} while (llTargetID = readID());
				return 1;
			}
			else if (strOption == "blackqq") {
				if (llTargetID == 0) {
					strReply = "当前黑名单用户列表：";
					for (auto each : BlackQQ) {
						strReply += "\n" + printQQ(each);
					}
					reply();
					return 1;
				}
				BlackMark mark;
				mark.llMap = { {"DiceMaid",DiceMaid},{"masterQQ",masterQQ} };
				mark.strMap = { {"type","other"},{"time",printSTime(stNow)} };
				if (!strReason.empty())mark.set("note", strReason);
				do{
					if (boolErase) {
						if (BlackQQ.count(llTargetID)) {
							rmBlackQQ(llTargetID, fromQQ);
						}
						else {
							reply(printQQ(llTargetID) + "并不在" + GlobalMsg["strSelfName"] + "的黑名单！");
						}
					}
					else {
						if (BlackQQ.count(llTargetID)) {
							reply(printQQ(llTargetID) + "已加入" + GlobalMsg["strSelfName"] + "的黑名单!");
						}
						else {
							mark.set("fromQQ", llTargetID);
							if(addBlackQQ(mark))AdminNotify("已将" + printQQ(llTargetID) + "加入" + GlobalMsg["strSelfName"] + "的黑名单√");;
						}
					}
				} while (llTargetID = readID());
				return 1;
			}
			else reply("有什么事么？" + getName(fromQQ));
			return 0;
		
		}
	}
	int MasterSet() {
		std::string strOption = readUntilSpace();
		if (strOption.empty()) {
			reply("有什么事么" + getName(fromQQ));
			return -1;
		}
		if (strOption == "groupclr") {
			std::string strPara = readRest();
			int intGroupCnt = clearGroup(strPara, fromQQ);
			return 1;
		}
		else if (strOption == "delete") {
			reply("你不再是" + GlobalMsg["strSelfName"] + "的Master！");
			masterQQ = 0;
			AdminQQ.clear();
			MonitorList.clear();
			return 1;
		}
		else if (strOption == "admin") {
			bool boolErase = false;
			readSkipSpace();
			if (strMsg[intMsgCnt] == '-') {
				boolErase = true;
				intMsgCnt++;
			}
			if (strMsg[intMsgCnt] == '+') {intMsgCnt++;}
			long long llAdmin = readID();
			if (llAdmin) {
				if (boolErase) {
					if (AdminQQ.count(llAdmin)) {
						AdminNotify("已移除管理员" + printQQ(llAdmin) + "√");
						MonitorList.erase({ llAdmin,Private });
						AdminQQ.erase(llAdmin);
					}
					else {
						reply("该用户不是管理员！");
					}
				}
				else {
					if (AdminQQ.count(llAdmin)) {
						reply("该用户已是管理员！");
					}
					else {
						AdminQQ.insert(llAdmin);
						MonitorList.insert({ llAdmin,Private });
						AdminNotify("已添加管理员" + printQQ(llAdmin) + "√");
					}
				}
				return 1;
			}
			else{
				strReply = "当前管理员：";
				for (auto it : AdminQQ) {
					strReply += "\n" + printQQ(it);
				}
				reply();
				return 1;
			}
		}
		else return AdminEvent(strOption);
		return 0;
	}
	int DiceReply() {
		if (strMsg[0] != '.')return 0;
		intMsgCnt++ ;
		int intT = (int)fromType;
		while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
			intMsgCnt++;
		const string strNickName = getName(fromQQ, fromGroup);
		isAuth = isAdmin || fromType != Group || getGroupMemberInfo(fromGroup, fromQQ).permissions > 1;
		strLowerMessage = strMsg;
		std::transform(strLowerMessage.begin(), strLowerMessage.end(), strLowerMessage.begin(), [](unsigned char c) { return tolower(c); });
		if (strLowerMessage.substr(intMsgCnt, 7) == "dismiss")
		{
			if (intT == PrivateT) {
				reply(GlobalMsg["strDismissPrivate"]);
				return 1;
			}
			intMsgCnt += 7;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string QQNum;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				QQNum += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (QQNum.empty() || (QQNum.length() == 4 && QQNum == to_string(getLoginQQ() % 10000)) || QQNum ==
				to_string(getLoginQQ()))
			{
				if (intT == DiscussT) {
					if (!GlobalMsg["strDismiss"].empty())reply(GlobalMsg["strDismiss"]);
					Sleep(100);
					setDiscussLeave(fromGroup);
					mLastMsgList.erase(fromChat);
				}
				else if (isAuth)
				{
					if (!GlobalMsg["strDismiss"].empty())reply(GlobalMsg["strDismiss"]);
					setGroupLeave(fromGroup);
					mLastMsgList.erase(fromChat);
				}
				else
				{
					reply(GlobalMsg["strPermissionDeniedErr"]);
				}
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 7) == "warning") {
			intMsgCnt += 7;
			string strWarning = readRest();
			if (strWarningList.count(strWarning))return 0;
			AddWarning(strWarning, fromQQ, fromGroup);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 6) == "master"&&boolMasterMode) {
			intMsgCnt += 6;
			if (masterQQ == 0) {
				masterQQ = fromQQ;
				AdminQQ.insert(masterQQ);
				MonitorList.insert({ masterQQ,Private });
				boolConsole["DisabledSend"] = true;
				reply("试问，你就是" + GlobalMsg["strSelfName"] + "的Master√");
			}
			else if (isMaster) {
				return MasterSet();
			}
			else {
				reply(GlobalMsg["strNotMaster"]);
				return true;
			}
			return 1;
		}
		if (BlackQQ.count(fromQQ) || (intT != PrivateT && BlackGroup.count(fromGroup))) {
			return 1;
		}
		if (strLowerMessage.substr(intMsgCnt, 3) == "bot")
		{
			intMsgCnt += 3;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string Command;
			while (intMsgCnt != strLowerMessage.length() && !isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !isspace(
				static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				Command += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string QQNum = strLowerMessage.substr(intMsgCnt, strMsg.find(' ', intMsgCnt) - intMsgCnt);
			if (QQNum.empty() || QQNum == to_string(getLoginQQ()) || (QQNum.length() == 4 && QQNum == to_string(
				getLoginQQ() % 10000)))
			{
				if (Command == "on")
				{
					if (boolConsole["DisabledGlobal"])reply(GlobalMsg["strGlobalOff"]);
					else if (fromType == Group) {
						if (isAuth)
						{
							if (DisabledGroup.count(fromGroup))
							{
								DisabledGroup.erase(fromGroup);
								reply(GlobalMsg["strBotOn"]);
							}
							else
							{
								reply(GlobalMsg["strBotOnAlready"]);
							}
						}
						else
						{
							reply(GlobalMsg["strPermissionDeniedErr"]);
						}
					}
					else if (fromType == Discuss) {
						if (DisabledDiscuss.count(fromGroup))
						{
							DisabledDiscuss.erase(fromGroup);
							reply(GlobalMsg["strBotOn"]);
						}
						else
						{
							reply(GlobalMsg["strBotOnAlready"]);
						}
					}
				}
				else if (Command == "off")
				{
					if (fromType == Group) {
						if (isAuth)
						{
							if (!DisabledGroup.count(fromGroup))
							{
								DisabledGroup.insert(fromGroup);
								reply(GlobalMsg["strBotOff"]);
							}
							else
							{
								reply(GlobalMsg["strBotOffAlready"]);
							}
						}
						else
						{
							reply(GlobalMsg["strPermissionDeniedErr"]);
						}
					}
					else if (fromType == Discuss) {
						if (!DisabledDiscuss.count(fromGroup))
						{
							DisabledDiscuss.insert(fromGroup);
							reply(GlobalMsg["strBotOff"]);
						}
						else
						{
							reply(GlobalMsg["strBotOffAlready"]);
						}
					}
				}
				else
				{
					reply(Dice_Full_Ver + GlobalMsg["strBotMsg"]);
				}
			}
			return 1;
		}
		if (isBotOff) {
			if (intT == PrivateT) {
				reply(GlobalMsg["strGlobalOff"]);
				return 1;
			}
			return 0; 
		}
		if (strLowerMessage.substr(intMsgCnt, 7) == "helpdoc"&&isAdmin)
		{
			intMsgCnt += 7;
			while (strMsg[intMsgCnt] == ' ')
				intMsgCnt++;
			if (intMsgCnt == strMsg.length()) {
				reply(GlobalMsg["strHlpNameEmpty"]);
				return true;
			}
			string strName;
			while (!isspace(static_cast<unsigned char>(strMsg[intMsgCnt])) && intMsgCnt != strMsg.length())
			{
				strName += strMsg[intMsgCnt];
				intMsgCnt++;
			}
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			if (intMsgCnt == strMsg.length()) {
				HelpDoc.erase(strName);
				reply(format(GlobalMsg["strHlpReset"], { strName }));
			}
			else{
				string strHelpdoc = strMsg.substr(intMsgCnt);
				EditedHelpDoc[strName] = strHelpdoc;
				HelpDoc[strName] = strHelpdoc;
				reply(format(GlobalMsg["strHlpSet"], { strName }));
			}
			string strFileLoc = getAppDirectory();
			ofstream ofstreamHelpDoc(strFileLoc + "HelpDoc.txt", ios::out | ios::trunc);
			for (auto it : EditedHelpDoc)
			{
				while (it.second.find("\r\n") != string::npos)it.second.replace(it.second.find("\r\n"), 2, "\\n");
				while (it.second.find('\n') != string::npos)it.second.replace(it.second.find('\n'), 1, "\\n");
				while (it.second.find('\r') != string::npos)it.second.replace(it.second.find('\r'), 1, "\\r");
				while (it.second.find("\t") != string::npos)it.second.replace(it.second.find("\t"), 1, "\\t");
				ofstreamHelpDoc << it.first << '\n' << it.second << '\n';
			}
			return true;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "help")
		{
			intMsgCnt += 4;
			while (strLowerMessage[intMsgCnt] == ' ')
				intMsgCnt++;
			const string strOption = strLowerMessage.substr(intMsgCnt);
			if (strOption == "on")
			{
				if (fromType == Group) {
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (DisabledHELPGroup.count(fromGroup))
						{
							DisabledHELPGroup.erase(fromGroup);
							reply("成功在本群中启用.help命令!");
						}
						else
						{
							reply("在本群中.help命令没有被禁用!");
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
				}
				else if (fromType == Discuss) {
					if (DisabledHELPDiscuss.count(fromGroup))
					{
						DisabledHELPDiscuss.erase(fromGroup);
						reply("成功在本群中启用.help命令!");
					}
					else
					{
						reply("在本群中.help命令没有被禁用!");
					}
				}
				return 1;
			}
			else if (strOption == "off")
			{
				if (fromType == Group) {
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (!DisabledHELPGroup.count(fromGroup))
						{
							DisabledHELPGroup.insert(fromGroup);
							reply("成功在本群中禁用.help命令!");
						}
						else
						{
							reply("在本群中.help命令没有被启用!");
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
				}
				else if (fromType == Discuss) {
					if (!DisabledHELPDiscuss.count(fromGroup))
					{
						DisabledHELPDiscuss.insert(fromGroup);
						reply("成功在本群中禁用.help命令!");
					}
					else
					{
						reply("在本群中.help命令没有被启用!");
					}
				}
				return 1;
			}
			if (DisabledHELPGroup.count(fromGroup))
			{
				reply(GlobalMsg["strHELPDisabledErr"]);
				return 1;
			}
			if (strOption.empty()) {
				reply(GlobalMsg["strHlpMsg"]);
			}
			else if (HelpDoc.count(strOption)) {
				string strReply = HelpDoc[strOption];
				while (strReply[0] == '&') {
					strReply = HelpDoc[strReply.substr(1)];
				}
				reply(strReply);
				return 1;
			}
			else {
				reply(GlobalMsg["strHlpNotFound"]);
			}
			return true;
		}
		else if (strLowerMessage.substr(intMsgCnt, 7) == "welcome")
		{
			if (intT != GroupT) {
				reply(GlobalMsg["strWelcomePrivate"]);
				return 1;
			}
			intMsgCnt += 7;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			if (isAuth)
			{
				string strWelcomeMsg = strMsg.substr(intMsgCnt);
				if (strWelcomeMsg.empty())
				{
					if (WelcomeMsg.count(fromGroup))
					{
						WelcomeMsg.erase(fromGroup);
						reply(GlobalMsg["strWelcomeMsgClearNotice"]);
					}
					else
					{
						reply(GlobalMsg["strWelcomeMsgClearErr"]);
					}
				}
				else
				{
					WelcomeMsg[fromGroup] = strWelcomeMsg;
					reply(GlobalMsg["strWelcomeMsgUpdateNotice"]);
				}
			}
			else
			{
				reply(GlobalMsg["strPermissionDeniedErr"]);
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 6) == "setcoc") {
		if (intT == GroupT && getGroupMemberInfo(fromGroup, fromQQ).permissions == 1) {
			reply(GlobalMsg["strPermissionDeniedErr"]);
			return 1;
		}
		string strRule=readDigit();
		if (strRule.empty()) {
			mDefaultCOC.erase(fromChat);
			reply(GlobalMsg["strDefaultCOCClr"]);
			return 1;
		}
		else{
			int intRule = stoi(strRule);
			switch (intRule) {
			case 0:
				reply(GlobalMsg["strDefaultCOCSet"] + "0 规则书\n出1大成功\n不满50出96-100大失败，满50出100大失败");
				break;
			case 1:
				reply(GlobalMsg["strDefaultCOCSet"] + "1\n不满50出1大成功，满50出1-5大成功\n不满50出96-100大失败，满50出100大失败");
				break;
			case 2:
				reply(GlobalMsg["strDefaultCOCSet"] + "2\n出1-5且<=成功率大成功\n出100或出96-99且>成功率大失败");
				break;
			case 3:
				reply(GlobalMsg["strDefaultCOCSet"] + "3\n出1-5大成功\n出96-100大失败");
				break;
			case 4:
				reply(GlobalMsg["strDefaultCOCSet"] + "4\n出1-5且<=十分之一大成功\n不满50出>=96+十分之一大失败，满50出100大失败");
				break;
			case 5:
				reply(GlobalMsg["strDefaultCOCSet"] + "5\n出1-2且<五分之一大成功\n不满50出96-100大失败，满50出99-100大失败");
				break;
			default:
				reply(GlobalMsg["strDefaultCOCNotFound"]);
				return 1;
				break;
			}
			mDefaultCOC[fromChat] = intRule;
			return 1;
		}
}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "admin") 
		{
			intMsgCnt += 5;
			return AdminEvent(readUntilSpace());
		}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "coc7d" || strLowerMessage.substr(intMsgCnt, 4) == "cocd")
		{
			string strReply = strNickName;
			COC7D(strReply);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "coc6d")
		{
			string strReply = strNickName;
			COC6D(strReply);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "group")
		{
			if (intT != GroupT)return 1;
			if (getGroupMemberInfo(fromGroup, fromQQ).permissions == 1) {
				reply(GlobalMsg["strPermissionDeniedErr"]);
				return 1;
			}
			intMsgCnt += 5;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string Command=readPara();
			string strReply;
			if (Command == "state") {
				time_t tNow = time(NULL);
				const int intTMonth = 30 * 24 * 60 * 60;
				set<string> sInact;
				set<string> sBlackQQ;
				for (auto each : getGroupMemberList(fromGroup)) {
					if (!each.LastMsgTime || tNow - each.LastMsgTime > intTMonth) {
						sInact.insert(each.GroupNick + "(" + to_string(each.QQID) + ")");
					}
					if (BlackQQ.count(each.QQID)) {
						sBlackQQ.insert(each.GroupNick + "(" + to_string(each.QQID) + ")");
					}
				}
				strReply += getGroupList()[fromGroup] + "——本群现状:\n"
					+ "群号:" + to_string(fromGroup) + "\n"
					+ GlobalMsg["strSelfName"] + "在本群状态：" + (DisabledGroup.count(fromGroup) ? "禁用" : "启用") + (boolConsole["DisabledGlobal"] ? "（全局静默中）" : "") + "\n"
					+ ".me：" + (DisabledMEGroup.count(fromGroup) ? "禁用" : "启用") + (boolConsole["DisabledMe"] ? "（全局禁用中）" : "") + "\n"
					+ ".jrrp：" + (DisabledJRRPGroup.count(fromGroup) ? "禁用" : "启用") + (boolConsole["DisabledJrrp"] ? "（全局禁用中）" : "") + "\n"
					+ (DisabledHELPGroup.count(fromGroup) ? "已禁用.help\n" : "") 
					+ (DisabledOBGroup.count(fromGroup) ? "已禁用旁观模式\n" : "")
					+ "COC房规：" + (mDefaultCOC.count({ fromGroup,Group }) ? to_string(mDefaultCOC[{ fromGroup, Group }])+"\n" : "未设置\n")
					+ (mGroupInviter.count(fromGroup) ? "邀请者" + printQQ(mGroupInviter[fromGroup]) + "\n" : "")
					+ "入群欢迎:" + (WelcomeMsg.count(fromGroup) ? "已设置" : "未设置")
					+ (sInact.size() ? "\n30天不活跃群员数：" + to_string(sInact.size()) : "");
				if (sBlackQQ.size()) {
					strReply += "\n" + GlobalMsg["strSelfName"] + "的黑名单成员:";
					for (auto each : sBlackQQ) {
						strReply += "\n" + each;
					}
				}

				reply(strReply);
				return 1;
			}
			if (getGroupMemberInfo(fromGroup, getLoginQQ()).permissions == 1) {
				reply(GlobalMsg["strSelfPermissionErr"]);
				return 1;
			}
			if (Command == "ban")
			{
				if (!isAdmin) {
					reply(GlobalMsg["strNotAdmin"]);
					return -1;
				}
				string QQNum = readDigit();
				if (QQNum.empty()) {
					reply(GlobalMsg["strQQIDEmpty"]);
					return -1;
				}
				long long llMemberQQ = stoll(QQNum);
				GroupMemberInfo Member = getGroupMemberInfo(fromGroup, llMemberQQ);
				if (Member.QQID == llMemberQQ)
				{
					if (Member.permissions > 1) {
						reply(GlobalMsg["strSelfPermissionErr"]);
						return 1;
					}
					string strMainDice = readDice();
					if (strMainDice.empty()) {
						reply(GlobalMsg["strValueErr"]);
						return -1;
					}
					const int intDefaultDice = DefaultDice.count(fromQQ) ? DefaultDice[fromQQ] : 100;
					RD rdMainDice(strMainDice, intDefaultDice);
					rdMainDice.Roll();
					int intDuration = rdMainDice.intTotal;
					if (setGroupBan(fromGroup, llMemberQQ, intDuration * 60) == 0)
						if (intDuration <= 0)
							reply("裁定" + getName(Member.QQID, fromGroup) + "解除禁言√");
						else reply("裁定" + getName(Member.QQID, fromGroup) + "禁言时长" + rdMainDice.FormCompleteString() + "分钟√");
					else reply("禁言失败×");
				}
				else reply("查无此人×");
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "reply") {
		intMsgCnt += 5;
		if (!isAdmin) {
			reply(GlobalMsg["strNotAdmin"]);
			return -1;
		}
		string strReplyName = readUntilSpace();
		vector<string> *Deck = NULL;
		if (strReplyName.empty()) {
			reply(GlobalMsg["strParaEmpty"]);
			return -1;
		}
		else {
			CardDeck::mReplyDeck[strReplyName] = {};
			Deck = &CardDeck::mReplyDeck[strReplyName];
		}
		while (intMsgCnt != strMsg.length()) {
			string item = readItem();
			if(!item.empty())Deck->push_back(item);
		}
		if (Deck->empty()) {
			reply(format(GlobalMsg["strReplyDel"], { strReplyName }));
			CardDeck::mReplyDeck.erase(strReplyName);
		}
		else reply(format(GlobalMsg["strReplySet"], { strReplyName }));
		return 1;
}
		else if (strLowerMessage.substr(intMsgCnt, 5) == "rules")
		{
			intMsgCnt += 5;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			if (strLowerMessage.substr(intMsgCnt, 3) == "set") {
				intMsgCnt += 3;
				while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])) || strMsg[intMsgCnt] == ':')
					intMsgCnt++;
				string strDefaultRule = strMsg.substr(intMsgCnt);
				if (strDefaultRule.empty()) {
					DefaultRule.erase(fromQQ);
					reply(GlobalMsg["strRuleReset"]);
				}
				else {
					for (auto& n : strDefaultRule)
						n = toupper(static_cast<unsigned char>(n));
					DefaultRule[fromQQ] = strDefaultRule;
					reply(GlobalMsg["strRuleSet"]);
				}
			}
			else {
				string strSearch = strMsg.substr(intMsgCnt);
				string strDefaultRule = DefaultRule[fromQQ];
				for (auto& n : strSearch)
					n = toupper(static_cast<unsigned char>(n));
				string strReturn;
				if (DefaultRule.count(fromQQ) && strSearch.find(':') == string::npos && GetRule::get(strDefaultRule, strSearch, strReturn)) {
					reply(strReturn);
				}
				else if (GetRule::analyze(strSearch, strReturn))
				{
					reply(strReturn);
				}
				else
				{
					reply(GlobalMsg["strRuleErr"] + strReturn);
				}
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "coc6")
		{
			intMsgCnt += 4;
			if (strLowerMessage[intMsgCnt] == 's')
				intMsgCnt++;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string strNum;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				strNum += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (strNum.length() > 2)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			const int intNum = stoi(strNum.empty() ? "1" : strNum);
			if (intNum > 10)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			if (intNum == 0)
			{
				reply(GlobalMsg["strCharacterCannotBeZero"]);
				return 1;
			}
			string strReply = strNickName;
			COC6(strReply, intNum);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "deck") {
		if (!isAdmin&&boolConsole["DisabledDeck"])
		{
			reply(GlobalMsg["strDisabledDeckGlobal"]);
			return 1;
		}
		intMsgCnt += 4;
		readSkipSpace();
		string strPara = readPara();
		vector<string> *DeckPro = NULL, *DeckTmp = NULL;
		if (intT != PrivateT && CardDeck::mGroupDeck.count(fromGroup)) {
			DeckPro = &CardDeck::mGroupDeck[fromGroup];
			DeckTmp = &CardDeck::mGroupDeckTmp[fromGroup];
		}
		else{
			if (CardDeck::mPrivateDeck.count(fromQQ)) {
				DeckPro = &CardDeck::mPrivateDeck[fromQQ];
				DeckTmp = &CardDeck::mPrivateDeckTmp[fromQQ];
			}
			//haichayixiangpanding
		}
		if (strPara == "show") {
			if (!DeckTmp) {
				reply(GlobalMsg["strDeckTmpNotFound"]);
				return 1;
			}
			if (DeckTmp->size() == 0) {
				reply(GlobalMsg["strDeckTmpEmpty"]);
				return 1;
			}
			string strReply = GlobalMsg["strDeckTmpShow"] + "\n";
			for (auto it : *DeckTmp) {
				it.length() > 10 ? strReply += it + "\n" : strReply += it + "|";
			}
			strReply.erase(strReply.end()-1);
			reply(strReply);
			return 1;
		}
		else if (intT == GroupT && getGroupMemberInfo(fromGroup, fromQQ).permissions == 1) {
			reply(GlobalMsg["strPermissionDeniedErr"]);
			return 1;
		}
		else if (strPara == "set") {
			string strDeckName = readPara();
			if (strDeckName.empty())strDeckName = readDigit();
			if (strDeckName.empty()) {
				reply(GlobalMsg["strDeckNameEmpty"]);
				return 1;
			}
			vector<string> DeckSet = {};
			switch (CardDeck::findDeck(strDeckName)) {
			case 1:
				DeckSet = CardDeck::mPublicDeck[strDeckName];
				break;
			case 2: {
				int intSize = stoi(strDeckName) + 1;
				if (intSize == 0) {
					reply(GlobalMsg["strNumCannotBeZero"]);
					return 1;
				}
				strDeckName = "数列1至" + strDeckName;
				while (--intSize) {
					DeckSet.push_back(to_string(intSize));
				}
				break;
			}
			case 0:
			default:
				reply(GlobalMsg["strDeckNotFound"]);
				return 1;
			}
			if (intT == PrivateT) {
				CardDeck::mPrivateDeck[fromQQ] = DeckSet;
			}
			else {
				CardDeck::mGroupDeck[fromGroup] = DeckSet;
			}
			reply(format(GlobalMsg["strDeckProSet"], { strDeckName }));
			return 1;
		}
		else if (strPara == "reset") {
			*DeckTmp = vector<string>(*DeckPro);
			reply(GlobalMsg["strDeckTmpReset"]);
			return 1;
		}
		else if (strPara == "clr") {
			if(intT == PrivateT){
				if (CardDeck::mPrivateDeck.count(fromQQ) == 0) {
					reply(GlobalMsg["strDeckProNull"]);
					return 1;
				}
				CardDeck::mPrivateDeck.erase(fromQQ);
				if (DeckTmp)DeckTmp->clear();
				reply(GlobalMsg["strDeckProClr"]);
			}
			else {
				if (CardDeck::mGroupDeck.count(fromGroup) == 0) {
					reply(GlobalMsg["strDeckProNull"]);
					return 1;
				}
				CardDeck::mGroupDeck.erase(fromGroup);
				if (DeckTmp)DeckTmp->clear();
				reply(GlobalMsg["strDeckProClr"]);
			}
			return 1;
		}
		else if (strPara == "new") {
			if (intT != PrivateT && WhiteGroup.count(fromGroup) == 0) {
				reply(GlobalMsg["strWhiteGroupDenied"]);
				return 1;
			}
			if (intT == PrivateT && WhiteQQ.count(fromQQ) == 0) {
				reply(GlobalMsg["strWhiteQQDenied"]);
				return 1;
			}
			if (intT == PrivateT) {
				CardDeck::mPrivateDeck[fromQQ] = {};
				DeckPro = &CardDeck::mPrivateDeck[fromQQ];
			}
			else {
				CardDeck::mGroupDeck[fromGroup] = {};
				DeckPro = &CardDeck::mGroupDeck[fromGroup];
			}
			while (intMsgCnt != strMsg.length()) {
				string item = readItem();
				if (!item.empty())DeckPro->push_back(item);
			}
			reply(GlobalMsg["strDeckProNew"]);
			return 1;
		}
}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "draw")
		{
		if (!isAdmin&&boolConsole["DisabledDraw"])
		{
			reply(GlobalMsg["strDisabledDrawGlobal"]);
			return 1;
		}
			intMsgCnt += 4;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			vector<string> ProDeck;
			vector<string> *TempDeck = NULL;
			string strDeckName = readPara();
			if (strDeckName.empty()) {
				if (intT != PrivateT && CardDeck::mGroupDeck.count(fromGroup)) {
					if (CardDeck::mGroupDeckTmp.count(fromGroup) == 0 || CardDeck::mGroupDeckTmp[fromGroup].size() == 0)CardDeck::mGroupDeckTmp[fromGroup] = vector<string>(CardDeck::mGroupDeck[fromGroup]);
					TempDeck = &CardDeck::mGroupDeckTmp[fromGroup];
				}
				else if (CardDeck::mPrivateDeck.count(fromQQ)) {
					if (CardDeck::mPrivateDeckTmp.count(fromQQ) == 0 || CardDeck::mPrivateDeckTmp[fromQQ].size() == 0)CardDeck::mPrivateDeckTmp[fromQQ] = vector<string>(CardDeck::mPrivateDeck[fromQQ]);
					TempDeck = &CardDeck::mPrivateDeckTmp[fromQQ];
				}
				else {
					reply(GlobalMsg["strDeckNameEmpty"]);
					return 1;
				}
			}
			else {
				int intFoundRes = CardDeck::findDeck(strDeckName);
				if (intFoundRes == 0) {
					strReply = "是说" + strDeckName + "?" + GlobalMsg["strDeckNotFound"];
					reply(strReply);
					return 1;
				}
				ProDeck = CardDeck::mPublicDeck[strDeckName];
				TempDeck = &ProDeck;
			}
			string strCardNum=readDigit();
			auto intCardNum = strCardNum.empty() ? 1 : stoi(strCardNum);
			if (intCardNum == 0)
			{
				reply(GlobalMsg["strNumCannotBeZero"]);
				return 1;
			}
			strReply = format(GlobalMsg["strDrawCard"], { strNickName , CardDeck::drawCard(*TempDeck) });
			while (--intCardNum && TempDeck->size()) {
				string strItem = CardDeck::drawCard(*TempDeck);
				(strItem.length() < 10) ? strReply += " | " : strReply += '\n';
				strReply += strItem;
				if (strReply.length() > 1000) {
					reply(strReply);
					strReply.clear();
				}
			}
			reply(strReply);
			if (intCardNum) {
				reply(GlobalMsg["strDeckEmpty"]);
				return 1;
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "init"&&intT)
		{
			intMsgCnt += 4;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))intMsgCnt++;
			if (strLowerMessage.substr(intMsgCnt, 3) == "clr")
			{
				if (ilInitList->clear(fromGroup))
					strReply = "成功清除先攻记录！";
				else
					strReply = "列表为空！";
				reply();
				return 1;
			}
			ilInitList->show(fromGroup, strReply);
			reply();
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "jrrp")
		{
			if (boolConsole["DisabledJrrp"]) {
				reply(GlobalMsg["strDisabledJrrpGlobal"]);
				return 1;
			}
			intMsgCnt += 4;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			const string Command = strLowerMessage.substr(intMsgCnt, strMsg.find(' ', intMsgCnt) - intMsgCnt);
			if (intT == GroupT) {
				if (Command == "on")
				{
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (DisabledJRRPGroup.count(fromGroup))
						{
							DisabledJRRPGroup.erase(fromGroup);
							reply("成功在本群中启用JRRP!");
						}
						else
						{
							reply("在本群中JRRP没有被禁用!");
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				if (Command == "off")
				{
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (!DisabledJRRPGroup.count(fromGroup))
						{
							DisabledJRRPGroup.insert(fromGroup);
							reply("成功在本群中禁用JRRP!");
						}
						else
						{
							reply("在本群中JRRP没有被启用!");
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				if (DisabledJRRPGroup.count(fromGroup))
				{
					reply("在本群中JRRP功能已被禁用");
					return 1;
				}
			}
			else if (intT == DiscussT) {
				if (Command == "on")
				{
					if (DisabledJRRPDiscuss.count(fromGroup))
					{
						DisabledJRRPDiscuss.erase(fromGroup);
						reply("成功在此多人聊天中启用JRRP!");
					}
					else
					{
						reply("在此多人聊天中JRRP没有被禁用!");
					}
					return 1;
				}
				if (Command == "off")
				{
					if (!DisabledJRRPDiscuss.count(fromGroup))
					{
						DisabledJRRPDiscuss.insert(fromGroup);
						reply("成功在此多人聊天中禁用JRRP!");
					}
					else
					{
						reply("在此多人聊天中JRRP没有被启用!");
					}
					return 1;
				}
				if (DisabledJRRPDiscuss.count(fromGroup))
				{
					reply("在此多人聊天中JRRP已被禁用!");
					return 1;
				}
			}
			string des;
			string data = "QQ=" + to_string(CQ::getLoginQQ()) + "&v=20190114" + "&QueryQQ=" + to_string(fromQQ);
			char *frmdata = new char[data.length() + 1];
			strcpy_s(frmdata, data.length() + 1, data.c_str());
			bool res = Network::POST("api.kokona.tech", "/jrrp", 5555, frmdata, des);
			delete[] frmdata;
			if (res)
			{
				reply(format(GlobalMsg["strJrrp"], { strNickName, des }));
			}
			else
			{
				reply(format(GlobalMsg["strJrrpErr"], { des }));
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "link") {
		intMsgCnt += 4;
		if (!isAdmin) {
			reply(GlobalMsg["strNotAdmin"]);
			return true;
		}
		isLinkOrder = true;
		string strOption = readPara();
		if (strOption == "close") {
			if (mLinkedList.count(fromChat)) {
				chatType ToChat = mLinkedList[fromChat];
				mLinkedList.erase(fromChat);
				auto Range = mFwdList.equal_range(fromChat);
				for (auto it = Range.first; it != Range.second; ++it) {
					if (it->second == ToChat) {
						mFwdList.erase(it);
						break;
					}
				}
				Range = mFwdList.equal_range(ToChat);
				for (auto it = Range.first; it != Range.second; ++it) {
					if (it->second == fromChat) {
						mFwdList.erase(it);
						break;
					}
				}
				reply(GlobalMsg["strLinkLoss"]);
				return 1;
			}
			return 1;
		}
		string strType = readPara();
		chatType ToChat;
		string strID = readDigit();
		if (strID.empty()) {
			reply(GlobalMsg["strLinkNotFound"]);
			return 1;
		}
		ToChat.first = stoll(strID);
		if (strType == "qq") {
			ToChat.second = Private;
		}
		else if (strType == "group") {
			ToChat.second = Group;
		}
		else if (strType == "discuss") {
			ToChat.second = Discuss;
		}
		else {
			reply(GlobalMsg["strLinkNotFound"]);
			return 1;
		}
		if (strOption == "with") {
			mLinkedList[fromChat] = ToChat;
			mFwdList.insert({ fromChat,ToChat });
			mFwdList.insert({ ToChat,fromChat });
		}
		else if (strOption == "from") {
			mLinkedList[fromChat] = ToChat;
			mFwdList.insert({ ToChat,fromChat });
		}
		else if (strOption == "to") {
			mLinkedList[fromChat] = ToChat;
			mFwdList.insert({ fromChat,ToChat });
		}
		else return 1;
		if (mLastMsgList.count(ToChat))reply(GlobalMsg["strLinked"]);
		else reply(GlobalMsg["strLinkWarning"]);
		return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "name")
		{
			intMsgCnt += 4;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;

			string type;
			while (isalpha(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				type += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}

			auto nameType = NameGenerator::Type::UNKNOWN;
			if (type == "cn")
				nameType = NameGenerator::Type::CN;
			else if (type == "en")
				nameType = NameGenerator::Type::EN;
			else if (type == "jp")
				nameType = NameGenerator::Type::JP;

			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;

			string strNum;
			while (isdigit(static_cast<unsigned char>(strMsg[intMsgCnt])))
			{
				strNum += strMsg[intMsgCnt];
				intMsgCnt++;
			}
			if (strNum.size() > 2)
			{
				reply(GlobalMsg["strNameNumTooBig"]);
				return 1;
			}
			int intNum = stoi(strNum.empty() ? "1" : strNum);
			if (intNum > 10)
			{
				reply(GlobalMsg["strNameNumTooBig"]);
				return 1;
			}
			if (intNum == 0)
			{
				reply(GlobalMsg["strNameNumCannotBeZero"]);
				return 1;
			}
			vector<string> TempNameStorage;
			while (TempNameStorage.size() != intNum)
			{
				string name = NameGenerator::getRandomName(nameType);
				if (find(TempNameStorage.begin(), TempNameStorage.end(), name) == TempNameStorage.end())
				{
					TempNameStorage.push_back(name);
				}
			}
			string strReply = format(GlobalMsg["strNameGenerator"], { strNickName }) + "\n";
			for (auto i = 0; i != TempNameStorage.size(); i++)
			{
				strReply.append(TempNameStorage[i]);
				if (i != TempNameStorage.size() - 1)strReply.append(", ");
			}
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 4) == "send") {
			intMsgCnt += 4;
			readSkipSpace();
			//先考虑Master带参数向指定目标发送
			if (isAdmin) {
				if (strLowerMessage.substr(intMsgCnt, 2) == "qq") {
					intMsgCnt += 2;
					string strQQ = readDigit();
					if (strQQ.empty()) {
						reply(GlobalMsg["strSendMsgIDEmpty"]);
					}
					else if (intMsgCnt == strMsg.length()) {
						reply(GlobalMsg["strSendMsgEmpty"]);
					}
					else{
						long long llQQ = stoll(strQQ);
						string strToMsg = readRest();
						AddMsgToQueue(strToMsg, llQQ, Private);
						reply(GlobalMsg["strSendMsg"]);
					}
					return 1;
				}
				else if (strLowerMessage.substr(intMsgCnt, 5) == "group") {
					intMsgCnt += 5;
					string strGroup = readDigit();
					if (strGroup.empty()) {
						reply(GlobalMsg["strSendMsgIDEmpty"]);
					}
					else if (intMsgCnt == strMsg.length()) {
						reply(GlobalMsg["strSendMsgEmpty"]);
					}
					else {
						long long llGroup = stoll(strGroup);
						string strToMsg = readRest();
						AddMsgToQueue(strToMsg, llGroup, Group);
						reply(GlobalMsg["strSendMsg"]);
					}
					return 1;
				}
				else if (strLowerMessage.substr(intMsgCnt, 7) == "discuss") {
					intMsgCnt += 7;
					string strDiscuss = readDigit();
					if (strDiscuss.empty()) {
						reply(GlobalMsg["strSendMsgIDEmpty"]);
					}
					else if (intMsgCnt == strMsg.length()) {
						reply(GlobalMsg["strSendMsgEmpty"]);
					}
					else {
						long long llDiscuss = stoll(strDiscuss);
						string strToMsg = readRest();
						AddMsgToQueue(strToMsg, llDiscuss, Discuss);
						reply(GlobalMsg["strSendMsg"]);
					}
					return 1;
				}
			}
			if (!masterQQ || !boolMasterMode) {
				reply(GlobalMsg["strSendMsgInvalid"]);
			}
			else if (boolConsole["DisabledSend"]) {
				reply(GlobalMsg["strDisabledSendGlobal"]);
			}
			else if (intMsgCnt == strMsg.length()) {
				reply(GlobalMsg["strSendMsgEmpty"]);
			}
			else {
				string strFwd = "来自" + printChat(fromChat);
				if (fromType == Private)strFwd += printQQ(fromQQ);
				if (masterQQ == fromQQ)strFwd.clear();
				strFwd += readRest();
				sendAdmin(strFwd);
				reply(GlobalMsg["strSendMasterMsg"]);
			}
			return 1;
}
		else if (strLowerMessage.substr(intMsgCnt, 3) == "coc")
		{
			intMsgCnt += 3;
			if (strLowerMessage[intMsgCnt] == '7')
				intMsgCnt++;
			if (strLowerMessage[intMsgCnt] == 's')
				intMsgCnt++;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string strNum;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				strNum += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (strNum.length() > 2)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			const int intNum = stoi(strNum.empty() ? "1" : strNum);
			if (intNum > 10)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			if (intNum == 0)
			{
				reply(GlobalMsg["strCharacterCannotBeZero"]);
				return 1;
			}
			string strReply = strNickName;
			COC7(strReply, intNum);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 3) == "dnd")
		{
			intMsgCnt += 3;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string strNum;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				strNum += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (strNum.length() > 2)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			const int intNum = stoi(strNum.empty() ? "1" : strNum);
			if (intNum > 10)
			{
				reply(GlobalMsg["strCharacterTooBig"]);
				return 1;
			}
			if (intNum == 0)
			{
				reply(GlobalMsg["strCharacterCannotBeZero"]);
				return 1;
			}
			string strReply = strNickName;
			DND(strReply, intNum);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 3) == "nnn")
		{
			intMsgCnt += 3;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			string type = strLowerMessage.substr(intMsgCnt, 2);
			string name;
			if (type == "cn")
				name = NameGenerator::getChineseName();
			else if (type == "en")
				name = NameGenerator::getEnglishName();
			else if (type == "jp")
				name = NameGenerator::getJapaneseName();
			else
				name = NameGenerator::getRandomName();
			Name->set(fromGroup, fromQQ, name);
			const string strReply = format(GlobalMsg["strNameSet"], { strNickName, strip(name) });
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 3) == "set")
		{
			intMsgCnt += 3;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string strDefaultDice = strLowerMessage.substr(intMsgCnt, strLowerMessage.find(" ", intMsgCnt) - intMsgCnt);
			while (strDefaultDice[0] == '0')
				strDefaultDice.erase(strDefaultDice.begin());
			if (strDefaultDice.empty())
				strDefaultDice = "100";
			for (auto charNumElement : strDefaultDice)
				if (!isdigit(static_cast<unsigned char>(charNumElement)))
				{
					reply(GlobalMsg["strSetInvalid"]);
					return 1;
				}
			if (strDefaultDice.length() > 3 && strDefaultDice != "1000")
			{
				reply(GlobalMsg["strSetTooBig"]);
				return 1;
			}
			const int intDefaultDice = stoi(strDefaultDice);
			if (intDefaultDice == 100)
				DefaultDice.erase(fromQQ);
			else
				DefaultDice[fromQQ] = intDefaultDice;
			const string strSetSuccessReply = "已将" + strNickName + "的默认骰类型更改为D" + strDefaultDice;
			reply(strSetSuccessReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 3) == "str"&&isAdmin) {
			string strName;
			while (!isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && intMsgCnt != strLowerMessage.length())
			{
				strName += strMsg[intMsgCnt];
				intMsgCnt++;
			}
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			if (GlobalMsg.count(strName)) {
				if (intMsgCnt == strMsg.length()) {
					EditedMsg.erase(strName);
					GlobalMsg[strName] = "";
					AdminNotify("已清除" + strName + "的自定义，但恢复默认设置需要重启应用。");
				}
				else {
					string strMessage = strMsg.substr(intMsgCnt);
					if (strMessage == "show") {
						reply(GlobalMsg[strName]);
						return 1;
					}
					if (strMessage == "NULL")strMessage = "";
					EditedMsg[strName] = strMessage;
					GlobalMsg[strName] = (strName == "strHlpMsg") ? Dice_Short_Ver + "\n" + strMessage : strMessage;
					isMaster ? reply("已自定义" + strName + "的文本") : AdminNotify("已自定义" + strName + "的文本");
				}
				SaveCustomMsg(string(getAppDirectory()) + "CustomMsg.json");
			}
			else {
				reply("是说" + strName + "？这似乎不是会用到的语句×");
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "en")
		{
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string strSkillName;
			while (intMsgCnt != strMsg.length() && !isdigit(static_cast<unsigned char>(strMsg[intMsgCnt])) && !isspace(static_cast<unsigned char>(strMsg[intMsgCnt]))
				)
			{
				strSkillName += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (SkillNameReplace.count(strSkillName))strSkillName = SkillNameReplace[strSkillName];
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			string strCurrentValue;
			while (isdigit(static_cast<unsigned char>(strMsg[intMsgCnt])))
			{
				strCurrentValue += strMsg[intMsgCnt];
				intMsgCnt++;
			}
			int intCurrentVal;
			if (strCurrentValue.empty())
			{
				if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[SourceType(
					fromQQ, intT, fromGroup)].count(strSkillName))
				{
					intCurrentVal = CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName];
				}
				else if (SkillDefaultVal.count(strSkillName))
				{
					intCurrentVal = SkillDefaultVal[strSkillName];
				}
				else
				{
					reply(GlobalMsg["strEnValEmpty"]);
					return 1;
				}
			}
			else
			{
				if (strCurrentValue.length() > 3)
				{
					reply(GlobalMsg["strEnValInvalid"]);

					return 1;
				}
				intCurrentVal = stoi(strCurrentValue);
			}
			readSkipSpace();
			//可变成长值表达式
			string strEnChange;
			string strEnFail;
			string strEnSuc;
			//以加减号做开头确保与技能值相区分
			if (strLowerMessage[intMsgCnt] == '+' || strLowerMessage[intMsgCnt] == '-') {
				strEnChange = strLowerMessage.substr(intMsgCnt, strMsg.find(' ', intMsgCnt) - intMsgCnt);
				//没有'/'时默认成功变化值
				if (strEnChange.find('/')!=std::string::npos) {
					strEnFail = strEnChange.substr(0, strEnChange.find("/"));
					strEnSuc = strEnChange.substr(strEnChange.find("/") + 1);
				}
				else strEnSuc = strEnChange;
			}
			if (strSkillName.empty())strSkillName = GlobalMsg["strRollEnSkillName"];
			string strAns = format(GlobalMsg["strRollEnSkill"], { strNickName ,strSkillName }) ;
			const int intTmpRollRes = RandomGenerator::Randint(1, 100);
			strAns += ":\n1D100=" + to_string(intTmpRollRes) + "/" + to_string(intCurrentVal);

			if (intTmpRollRes <= intCurrentVal && intTmpRollRes <= 95)
			{
				if(strEnFail.empty())strAns += " 失败!\n你的" + strSkillName + "没有变化!";
				else {
					RD rdEnFail(strEnFail);
					if (rdEnFail.Roll()) {
						reply(GlobalMsg["strValueErr"]);
						return 1;
					}
					if (strCurrentValue.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = intCurrentVal +
							rdEnFail.intTotal;
					}
					strAns += " 失败!\n你的" + strSkillName + "变化" + rdEnFail.FormCompleteString() + "点，当前为" + to_string(intCurrentVal +
						rdEnFail.intTotal) + "点";
				}
			}
			else
			{
				if(strEnSuc.empty()){
					strAns += " 成功!\n你的" + strSkillName + "增加1D10=";
					const int intTmpRollD10 = RandomGenerator::Randint(1, 10);
					strAns += to_string(intTmpRollD10) + "点,当前为" + to_string(intCurrentVal + intTmpRollD10) + "点";
					if (strCurrentValue.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = intCurrentVal +
							intTmpRollD10;
					}
				}
				else {
					RD rdEnSuc(strEnSuc);
					if (rdEnSuc.Roll()) {
						reply(GlobalMsg["strValueErr"]);
						return 1;
					}
					if (strCurrentValue.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = intCurrentVal +
							rdEnSuc.intTotal;
					}
					strAns += " 成功!\n你的" + strSkillName + "变化" + rdEnSuc.FormCompleteString() + "点，当前为" + to_string(intCurrentVal +
						rdEnSuc.intTotal) + "点";
				}
			}
			reply(strAns);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "li")
		{
			string strAns = strNickName + "的疯狂发作-总结症状:\n";
			LongInsane(strAns);
			reply(strAns);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "me")
		{
			if (!isAdmin&&boolConsole["DisabledMe"])
			{
				reply(GlobalMsg["strDisabledMeGlobal"]);
				return 1;
			}
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			if (intT == 0) {
				string strGroupID;
				while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				{
					strGroupID += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
				while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
					intMsgCnt++;
				string strAction = strip(strMsg.substr(intMsgCnt));

				for (auto i : strGroupID)
				{
					if (!isdigit(static_cast<unsigned char>(i)))
					{
						reply(GlobalMsg["strGroupIDInvalid"]);
						return 1;
					}
				}
				if (strGroupID.empty())
				{
					reply(GlobalMsg["strGroupIDEmpty"]);
					return 1;
				}
				if (strAction.empty())
				{
					reply(GlobalMsg["strActionEmpty"]);
					return 1;
				}
				const long long llGroupID = stoll(strGroupID);
				if (DisabledGroup.count(llGroupID))
				{
					reply(GlobalMsg["strDisabledErr"]);
					return 1;
				}
				if (DisabledMEGroup.count(llGroupID))
				{
					reply(GlobalMsg["strMEDisabledErr"]);
					return 1;
				}
				string strReply = getName(fromQQ, llGroupID) + strAction;
				const int intSendRes = sendGroupMsg(llGroupID, strReply);
				if (intSendRes < 0)
				{
					reply(GlobalMsg["strSendErr"]);
				}
				else
				{
					reply(GlobalMsg["strSendSuccess"]);
				}
				return 1;
			}
			string strAction = strLowerMessage.substr(intMsgCnt);
			if (intT == GroupT) {
				if (strAction == "on")
				{
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (DisabledMEGroup.count(fromGroup))
						{
							DisabledMEGroup.erase(fromGroup);
							reply(GlobalMsg["strMeOn"]);
						}
						else
						{
							reply(GlobalMsg["strMeOnAlready"]);
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				if (strAction == "off")
				{
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (!DisabledMEGroup.count(fromGroup))
						{
							DisabledMEGroup.insert(fromGroup);
							reply(GlobalMsg["strMeOff"]);
						}
						else
						{
							reply(GlobalMsg["strMeOffAlready"]);
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				if (DisabledMEGroup.count(fromGroup))
				{
					reply(GlobalMsg["strMEDisabledErr"]);
					return 1;
				}
			}
			else if (intT == DiscussT) {
				if (strAction == "on")
				{
					if (DisabledMEDiscuss.count(fromGroup))
					{
						DisabledMEDiscuss.erase(fromGroup);
						reply(GlobalMsg["strMeOn"]);
					}
					else
					{
						reply(GlobalMsg["strMeOnAlready"]);
					}
					return 1;
				}
				if (strAction == "off")
				{
					if (!DisabledMEDiscuss.count(fromGroup))
					{
						DisabledMEDiscuss.insert(fromGroup);
						reply(GlobalMsg["strMeOff"]);
					}
					else
					{
						reply(GlobalMsg["strMeOffAlready"]);
					}
					return 1;
				}
				if (DisabledMEDiscuss.count(fromGroup))
				{
					reply(GlobalMsg["strMEDisabledErr"]);
					return 1;
				}
			}
			strAction = strip(strMsg.substr(intMsgCnt));
			if (strAction.empty())
			{
				reply(GlobalMsg["strActionEmpty"]);
				return 1;
			}
			const string strReply = strNickName + strAction;
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "nn")
		{
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			string name = strip(strMsg.substr(intMsgCnt));
			if (name.length() > 50)
			{
				reply(GlobalMsg["strNameTooLongErr"]);
				return 1;
			}
			if (!name.empty())
			{
				Name->set(fromGroup, fromQQ, name);
				const string strReply = format(GlobalMsg["strNameSet"], { strNickName, strip(name) });
				reply(strReply);
			}
			else
			{
				if (Name->del(fromGroup, fromQQ))
				{
					const string strReply = format(GlobalMsg["strNameClr"], { strNickName });
					reply(strReply);
				}
				else
				{
					const string strReply = strNickName + GlobalMsg["strNameDelErr"];
					reply(strReply);
				}
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "ob")
		{
			if (intT == PrivateT) {
				reply(GlobalMsg["strObPrivate"]);
				return 1;
			}
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			const string Command = strLowerMessage.substr(intMsgCnt, strMsg.find(' ', intMsgCnt) - intMsgCnt);
			if (Command == "on")
			{
				if (intT == GroupT) {
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (DisabledOBGroup.count(fromGroup))
						{
							DisabledOBGroup.erase(fromGroup);
							reply(GlobalMsg["strObOn"]);
						}
						else
						{
							reply(GlobalMsg["strObOnAlready"]);
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				else {
					if (DisabledOBDiscuss.count(fromGroup))
					{
						DisabledOBDiscuss.erase(fromGroup);
						reply(GlobalMsg["strObOn"]);
					}
					else
					{
						reply(GlobalMsg["strObOnAlready"]);
					}
					return 1;
				}
			}
			if (Command == "off")
			{
				if (intT == Group) {
					if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
					{
						if (!DisabledOBGroup.count(fromGroup))
						{
							DisabledOBGroup.insert(fromGroup);
							ObserveGroup.erase(fromGroup);
							reply(GlobalMsg["strObOff"]);
						}
						else
						{
							reply(GlobalMsg["strObOffAlready"]);
						}
					}
					else
					{
						reply(GlobalMsg["strPermissionDeniedErr"]);
					}
					return 1;
				}
				else {
					if (!DisabledOBDiscuss.count(fromGroup))
					{
						DisabledOBDiscuss.insert(fromGroup);
						ObserveDiscuss.clear();
						reply(GlobalMsg["strObOff"]);
					}
					else
					{
						reply(GlobalMsg["strObOffAlready"]);
					}
					return 1;
				}
			}
			if (intT == GroupT && DisabledOBGroup.count(fromGroup))
			{
				reply(GlobalMsg["strObOffAlready"]);
				return 1;
			}
			if (intT == DiscussT && DisabledOBDiscuss.count(fromGroup))
			{
				reply(GlobalMsg["strObOffAlready"]);
				return 1;
			}
			if (Command == "list")
			{
				string Msg = GlobalMsg["strObList"];
				const auto Range = (intT == GroupT) ? ObserveGroup.equal_range(fromGroup) : ObserveDiscuss.equal_range(fromGroup);
				for (auto it = Range.first; it != Range.second; ++it)
				{
					Msg += "\n" + getName(it->second, fromGroup) + "(" + to_string(it->second) + ")";
				}
				const string strReply = Msg == GlobalMsg["strObList"] ? GlobalMsg["strObListEmpty"] : Msg;
				reply(strReply);
			}
			else if (Command == "clr")
			{
				if (intT == DiscussT) {
					ObserveDiscuss.erase(fromGroup);
					reply(GlobalMsg["strObListClr"]);
				}
				else if (getGroupMemberInfo(fromGroup, fromQQ).permissions >= 2)
				{
					ObserveGroup.erase(fromGroup);
					reply(GlobalMsg["strObListClr"]);
				}
				else
				{
					reply(GlobalMsg["strPermissionDeniedErr"]);
				}
			}
			else if (Command == "exit")
			{
				const auto Range = (intT == GroupT) ? ObserveGroup.equal_range(fromGroup) : ObserveDiscuss.equal_range(fromGroup);
				for (auto it = Range.first; it != Range.second; ++it)
				{
					if (it->second == fromQQ)
					{
						(intT == GroupT) ? ObserveGroup.erase(it) : ObserveDiscuss.erase(it);
						const string strReply = strNickName + GlobalMsg["strObExit"];
						reply(strReply);
						return 1;
					}
				}
				const string strReply = strNickName + GlobalMsg["strObExitAlready"];
				reply(strReply);
			}
			else
			{
				const auto Range = (intT == GroupT) ? ObserveGroup.equal_range(fromGroup) : ObserveDiscuss.equal_range(fromGroup);
				for (auto it = Range.first; it != Range.second; ++it)
				{
					if (it->second == fromQQ)
					{
						const string strReply = strNickName + GlobalMsg["strObEnterAlready"];
						reply(strReply);
						return 1;
					}
				}
				(intT == GroupT) ? ObserveGroup.insert(make_pair(fromGroup, fromQQ)) : ObserveDiscuss.insert(make_pair(fromGroup, fromQQ));
				const string strReply = strNickName + GlobalMsg["strObEnter"];
				reply(strReply);
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "ra"|| strLowerMessage.substr(intMsgCnt, 2) == "rc")
		{
			intMsgCnt += 2;
			int intRule = mDefaultCOC.count(fromChat) ? mDefaultCOC[fromChat] : 0;
			int intTurnCnt = 1;
			if (strMsg.find("#") != string::npos)
			{
				string strTurnCnt = strMsg.substr(intMsgCnt, strMsg.find("#") - intMsgCnt);
				//#能否识别有效
				if (strTurnCnt.empty())intMsgCnt++;
				else if (strTurnCnt.length() == 1 && isdigit(static_cast<unsigned char>(strTurnCnt[0])) || strTurnCnt == "10") {
					intMsgCnt += strTurnCnt.length() + 1;
					intTurnCnt = stoi(strTurnCnt);
				}
			}
			string strSkillName;
			string strMainDice = "D100";
			string strSkillModify;
			//困难等级
			string strDifficulty;
			int intDifficulty = 1;
			int intSkillModify = 0;
			//乘数
			int intSkillMultiple = 1;
			//除数
			int intSkillDivisor = 1;
			//自动成功
			bool isAutomatic = false;
			if (strLowerMessage[intMsgCnt] == 'p' || strLowerMessage[intMsgCnt] == 'b') {
				strMainDice = strLowerMessage[intMsgCnt];
				intMsgCnt++;
				while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt]))) {
					strMainDice += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
			}
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))intMsgCnt++;
			while (intMsgCnt != strLowerMessage.length() && !isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !
				isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && strLowerMessage[intMsgCnt] != '=' && strLowerMessage[intMsgCnt] !=
				':' && strLowerMessage[intMsgCnt] != '+' && strLowerMessage[intMsgCnt] != '-' && strLowerMessage[intMsgCnt] != '*' && strLowerMessage[intMsgCnt] != '/')
			{
				strSkillName += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (strSkillName.find("自动成功") == 0) {
				strDifficulty = strSkillName.substr(0, 8);
				strSkillName = strSkillName.substr(8);
				isAutomatic = true;
			}
			if (strSkillName.find("困难") == 0 || strSkillName.find("极难") == 0) {
				strDifficulty += strSkillName.substr(0, 4);
				intDifficulty = (strSkillName.substr(0, 4) == "困难") ? 2 : 5;
				strSkillName=strSkillName.substr(4);
			}
			if (SkillNameReplace.count(strSkillName))strSkillName = SkillNameReplace[strSkillName];
			if (strLowerMessage[intMsgCnt] == '*') {
				intMsgCnt++;
				intSkillMultiple = stoi(readDigit());
			}
			while (strLowerMessage[intMsgCnt] == '+' || strLowerMessage[intMsgCnt] == '-') {
				strSkillModify += strLowerMessage[intMsgCnt];
				intMsgCnt++;
				while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt]))) {
					strSkillModify += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
				intSkillModify = stoi(strSkillModify);
			}
			if (strLowerMessage[intMsgCnt] == '/') {
				intMsgCnt++;
				intSkillDivisor = stoi(readDigit());
				if (intSkillDivisor == 0) {
					reply(GlobalMsg["strValueErr"]);
					return 1;
				}
			}
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) || strLowerMessage[intMsgCnt] == '=' || strLowerMessage[intMsgCnt] ==
				':')intMsgCnt++;
			string strSkillVal;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				strSkillVal += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				intMsgCnt++;
			}
			string strReason = strMsg.substr(intMsgCnt);
			int intSkillVal;
			if (strSkillVal.empty())
			{
				if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[SourceType(
					fromQQ, intT, fromGroup)].count(strSkillName))
				{
					intSkillVal = CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName];
				}
				else if (SkillDefaultVal.count(strSkillName))
				{
					intSkillVal = SkillDefaultVal[strSkillName];
				}
				else
				{
					reply(GlobalMsg["strUnknownPropErr"]);
					return 1;
				}
			}
			else if (strSkillVal.length() > 3)
			{
				reply(GlobalMsg["strPropErr"]);
				return 1;
			}
			else
			{
				intSkillVal = stoi(strSkillVal);
			}
			int intFianlSkillVal = (intSkillVal * intSkillMultiple + intSkillModify)/ intSkillDivisor/ intDifficulty;
			if (intFianlSkillVal < 0 || intFianlSkillVal > 1000)
			{
				reply(GlobalMsg["strSuccessRateErr"]);
				return 1;
			}
			RD rdMainDice(strMainDice);
			const int intFirstTimeRes = rdMainDice.Roll();
			if (intFirstTimeRes == ZeroDice_Err)
			{
				reply(GlobalMsg["strZeroDiceErr"]);
				return 1;
			}
			if (intFirstTimeRes == DiceTooBig_Err)
			{
				reply(GlobalMsg["strDiceTooBigErr"]);
				return 1;
			}
			string strModifiedSkill = strDifficulty + strSkillName + ((intSkillMultiple != 1) ? "×" + to_string(intSkillMultiple) : "") + strSkillModify + ((intSkillDivisor != 1) ? "/" + to_string(intSkillDivisor) : "");
			strReply = format(GlobalMsg["strRollSkill"], { strNickName ,strModifiedSkill });
			if (!strReason.empty())
			{
				strReply = format(GlobalMsg["strRollSkillReason"], { strNickName ,strModifiedSkill ,strReason });
			}
			if (intTurnCnt > 1) {
				strReply += ":";
				while (intTurnCnt--) {
					rdMainDice.Roll();
					strReply += "\n" + rdMainDice.FormCompleteString() + "/" + to_string(intFianlSkillVal) + " ";
					int intRes = RollSuccessLevel(rdMainDice.intTotal, intFianlSkillVal, intRule);
					switch (intRes) {
					case 0:strReply += GlobalMsg["strFumble"]; break;
					case 1:strReply += isAutomatic ? GlobalMsg["strSuccess"] : GlobalMsg["strFailure"]; break;
					case 5:strReply += GlobalMsg["strCriticalSuccess"]; break;
					case 4:if (intDifficulty == 1) { strReply += GlobalMsg["strExtremeSuccess"]; break; }
					case 3:if (intDifficulty == 1) { strReply += GlobalMsg["strHardSuccess"]; break; }
					case 2:strReply += GlobalMsg["strSuccess"]; break;
					}
				}
				reply();
				return 1;
			}
			strReply += ":" + rdMainDice.FormCompleteString() + "/" + to_string(intFianlSkillVal) + " ";
			int intRes = RollSuccessLevel(rdMainDice.intTotal, intFianlSkillVal, intRule);
			switch (intRes) {
			case 0:strReply += GlobalMsg["strRollFumble"]; break;
			case 1:strReply += isAutomatic ? GlobalMsg["strRollRegularSuccess"] : GlobalMsg["strRollFailure"]; break;
			case 5:strReply += GlobalMsg["strRollCriticalSuccess"]; break;
			case 4:if (intDifficulty == 1) { strReply += GlobalMsg["strRollExtremeSuccess"]; break; }
			case 3:if (intDifficulty == 1) { strReply += GlobalMsg["strRollHardSuccess"]; break; }
			case 2:strReply += GlobalMsg["strRollRegularSuccess"]; break;
			}
			reply();
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "ri"&&intT)
		{
			intMsgCnt += 2;
			readSkipSpace();
			string strinit = "D20";
			if (strLowerMessage[intMsgCnt] == '+' || strLowerMessage[intMsgCnt] == '-')
			{
				strinit += readDice();
			}
			else if (isRollDice()){
				strinit = readDice();
			}
			readSkipSpace();
			string strname = strMsg.substr(intMsgCnt);
			if (strname.empty())
				strname = strNickName;
			else
				strname = strip(strname);
			RD initdice(strinit, 20);
			const int intFirstTimeRes = initdice.Roll();
			if (intFirstTimeRes == Value_Err)
			{
				reply(GlobalMsg["strValueErr"]);
				return 1;
			}
			if (intFirstTimeRes == Input_Err)
			{
				reply(GlobalMsg["strInputErr"]);
				return 1;
			}
			if (intFirstTimeRes == ZeroDice_Err)
			{
				reply(GlobalMsg["strZeroDiceErr"]);
				return 1;
			}
			if (intFirstTimeRes == ZeroType_Err)
			{
				reply(GlobalMsg["strZeroTypeErr"]);
				return 1;
			}
			if (intFirstTimeRes == DiceTooBig_Err)
			{
				reply(GlobalMsg["strDiceTooBigErr"]);
				return 1;
			}
			if (intFirstTimeRes == TypeTooBig_Err)
			{
				reply(GlobalMsg["strTypeTooBigErr"]);
				return 1;
			}
			if (intFirstTimeRes == AddDiceVal_Err)
			{
				reply(GlobalMsg["strAddDiceValErr"]);
				return 1;
			}
			if (intFirstTimeRes != 0)
			{
				reply(GlobalMsg["strUnknownErr"]);
				return 1;
			}
			ilInitList->insert(fromGroup, initdice.intTotal, strname);
			const string strReply = strname + "的先攻骰点：" + strinit + '=' + to_string(initdice.intTotal);
			reply(strReply);
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "sc")
		{
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string SanCost = strLowerMessage.substr(intMsgCnt, strMsg.find(' ', intMsgCnt) - intMsgCnt);
			intMsgCnt += SanCost.length();
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			string San;
			while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
			{
				San += strLowerMessage[intMsgCnt];
				intMsgCnt++;
			}
			if (SanCost.empty() || SanCost.find("/") == string::npos)
			{
				reply(GlobalMsg["strSCInvalid"]);
				return 1;
			}
			if (San.empty() && !(CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[
				SourceType(fromQQ, intT, fromGroup)].count("理智")))
			{
				reply(GlobalMsg["strSanEmpty"]);
				return 1;
			}
				for (const auto& character : SanCost.substr(0, SanCost.find("/")))
				{
					if (!isdigit(static_cast<unsigned char>(character)) && character != 'D' && character != 'd' && character != '+' && character != '-')
					{
						reply(GlobalMsg["strSCInvalid"]);
						return 1;
					}
				}
				for (const auto& character : SanCost.substr(SanCost.find("/") + 1))
				{
					if (!isdigit(static_cast<unsigned char>(character)) && character != 'D' && character != 'd' && character != '+' && character != '-')
					{
						reply(GlobalMsg["strSCInvalid"]);
						return 1;
					}
				}
				RD rdSuc(SanCost.substr(0, SanCost.find("/")));
				RD rdFail(SanCost.substr(SanCost.find("/") + 1));
				if (rdSuc.Roll() != 0 || rdFail.Roll() != 0)
				{
					reply(GlobalMsg["strSCInvalid"]);
					return 1;
				}
				if (San.length() >= 3)
				{
					reply(GlobalMsg["strSanInvalid"]);
					return 1;
				}
				const int intSan = San.empty() ? CharacterProp[SourceType(fromQQ, intT, fromGroup)]["理智"] : stoi(San);
				if (intSan == 0)
				{
					reply(GlobalMsg["strSanInvalid"]);
					return 1;
				}
				string strAns = format(GlobalMsg["strRollSc"], { strNickName });
				const int intTmpRollRes = RandomGenerator::Randint(1, 100);
				strAns += ":\n1D100=" + to_string(intTmpRollRes) + "/" + to_string(intSan);
				//调用房规
				int intRule = mDefaultCOC.count(fromChat) ? mDefaultCOC[fromChat] : 0;
				switch (RollSuccessLevel(intTmpRollRes, intSan, intRule)) {
				case 5:
				case 4:
				case 3:
				case 2:
					strAns += " 成功\n你的San值减少" + SanCost.substr(0, SanCost.find("/"));
					if (SanCost.substr(0, SanCost.find("/")).find("d") != string::npos)
						strAns += "=" + to_string(rdSuc.intTotal);
					strAns += +"点,当前剩余" + to_string(max(0, intSan - rdSuc.intTotal)) + "点";
					if (San.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)]["理智"] = max(0, intSan - rdSuc.intTotal);
					}
					break;
				case 1:
					strAns += " 失败\n你的San值减少" + SanCost.substr(SanCost.find("/") + 1);
					if (SanCost.substr(SanCost.find("/") + 1).find("d") != string::npos)
						strAns += "=" + to_string(rdFail.intTotal);
					strAns += +"点,当前剩余" + to_string(max(0, intSan - rdFail.intTotal)) + "点";
					if (San.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)]["理智"] = max(0, intSan - rdFail.intTotal);
					}
					break;
				case 0:
					strAns += " " + GlobalMsg["strRollFumble"] + "\n你的San值减少" + SanCost.substr(SanCost.find("/") + 1);
					// ReSharper disable once CppExpressionWithoutSideEffects
					rdFail.Max();
					if (SanCost.substr(SanCost.find("/") + 1).find("d") != string::npos)
						strAns += "最大值=" + to_string(rdFail.intTotal);
					strAns += +"点,当前剩余" + to_string(max(0, intSan - rdFail.intTotal)) + "点";
					if (San.empty())
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)]["理智"] = max(0, intSan - rdFail.intTotal);
					}
					break;
				}
				reply(strAns);
				return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "st")
		{
			intMsgCnt += 2;
			while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
				intMsgCnt++;
			if (intMsgCnt == strLowerMessage.length())
			{
				reply(GlobalMsg["strStErr"]);
				return 1;
			}
			if (strLowerMessage.substr(intMsgCnt, 3) == "clr")
			{
				if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)))
				{
					CharacterProp.erase(SourceType(fromQQ, intT, fromGroup));
				}
				reply(GlobalMsg["strPropCleared"]);
				return 1;
			}
			if (strLowerMessage.substr(intMsgCnt, 3) == "del")
			{
				intMsgCnt += 3;
				while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
					intMsgCnt++;
				string strSkillName;
				while (intMsgCnt != strLowerMessage.length() && !isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !(strLowerMessage[
					intMsgCnt] == '|'))
				{
					strSkillName += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
					if (SkillNameReplace.count(strSkillName))strSkillName = SkillNameReplace[strSkillName];
					if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[SourceType(
						fromQQ, intT, fromGroup)].count(strSkillName))
					{
						CharacterProp[SourceType(fromQQ, intT, fromGroup)].erase(strSkillName);
						reply(GlobalMsg["strPropDeleted"]);
					}
					else
					{
						reply(GlobalMsg["strPropNotFound"]);
					}
					return 1;
			}
			if (strLowerMessage.substr(intMsgCnt, 4) == "show")
			{
				intMsgCnt += 4;
				while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))
					intMsgCnt++;
				string strSkillName;
				while (intMsgCnt != strLowerMessage.length() && !isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !(strLowerMessage[
					intMsgCnt] == '|'))
				{
					strSkillName += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
					SourceType sCharProp = SourceType(fromQQ, intT, fromGroup);
					if (strSkillName.empty() && CharacterProp.count(sCharProp)) {
						string strReply = strNickName + "的属性列表：";
						for (auto each : CharacterProp[sCharProp]) {
							strReply += " " + each.first + ":" + to_string(each.second);
						}
						reply(strReply);
						return 1;
					}
					if (SkillNameReplace.count(strSkillName))strSkillName = SkillNameReplace[strSkillName];
					if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[SourceType(
						fromQQ, intT, fromGroup)].count(strSkillName))
					{
						reply(format(GlobalMsg["strProp"], {
							strNickName, strSkillName,
							to_string(CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName])
							}));
					}
					else if (SkillDefaultVal.count(strSkillName))
					{
						reply(format(GlobalMsg["strProp"], { strNickName, strSkillName, to_string(SkillDefaultVal[strSkillName]) }));
					}
					else
					{
						reply(GlobalMsg["strPropNotFound"]);
					}
					return 1;
			}
			bool boolError = false;
			bool isDetail = false;
			bool isModify = false;
			while (intMsgCnt != strLowerMessage.length())
			{
				string strSkillName;
				while (intMsgCnt != strLowerMessage.length() && !isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !
					isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && strLowerMessage[intMsgCnt] != '=' && strLowerMessage[intMsgCnt]
					!= ':' && strLowerMessage[intMsgCnt] != '-' && strLowerMessage[intMsgCnt] != '+')
				{
					strSkillName += strLowerMessage[intMsgCnt];
					intMsgCnt++;
				}
				if (SkillNameReplace.count(strSkillName))strSkillName = SkillNameReplace[strSkillName];
				while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) || strLowerMessage[intMsgCnt] == '=' || strLowerMessage[intMsgCnt
				] == ':')intMsgCnt++;
				if (strLowerMessage[intMsgCnt] == '-' || strLowerMessage[intMsgCnt] == '+') {
					char chSign = strLowerMessage[intMsgCnt];
					isDetail = true;
					isModify = true;
					intMsgCnt++;
					int intCurrentVal;
					if (CharacterProp.count(SourceType(fromQQ, intT, fromGroup)) && CharacterProp[SourceType(
						fromQQ, intT, fromGroup)].count(strSkillName))
					{
						intCurrentVal = CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName];
					}
					else if (SkillDefaultVal.count(strSkillName))
					{
						intCurrentVal = SkillDefaultVal[strSkillName];
					}
					else
					{
						reply(format(GlobalMsg["strStValEmpty"], {strSkillName}));
						return 1;
					}
					RD Mod(to_string(intCurrentVal) + chSign + readDice());
					if (Mod.Roll()) {
						reply(GlobalMsg["strValueErr"]);
						return 1;
					}
					else 
					{
						strReply += "\n" + strSkillName + "：" + Mod.FormCompleteString();
						if (Mod.intTotal < 0) {
							strReply += "→0";
							CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = 0;
						}
						else if (Mod.intTotal > 1000) {
							strReply += "→1000";
							CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = 1000;
						}
						else CharacterProp[SourceType(fromQQ, intT, fromGroup)][strSkillName] = Mod.intTotal;
					}
					while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) || strLowerMessage[intMsgCnt] == '|')intMsgCnt++;
					continue;
				}
				string strSkillVal = readDigit();
				if (strSkillName.empty() || strSkillVal.empty() || strSkillVal.length() > 3)
				{
					boolError = true;
					break;
				}
				int intSkillVal = stoi(strSkillVal);
				if (!SkillDefaultVal.count(strSkillName) || SkillDefaultVal[strSkillName] != intSkillVal)CharacterProp[SourceType(fromQQ, GroupT, fromGroup)][strSkillName] = intSkillVal;
				else if (CharacterProp[SourceType(fromQQ, GroupT, fromGroup)].count(strSkillName))CharacterProp[SourceType(fromQQ, GroupT, fromGroup)].erase(strSkillName);
				while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) || strLowerMessage[intMsgCnt] == '|')intMsgCnt++;
			}
			if (boolError)
			{
				reply(GlobalMsg["strPropErr"]);
			}
			else if(isModify){
				reply(format(GlobalMsg["strStModify"], { strNickName }) + strReply);
			}
			else
			{
				reply(GlobalMsg["strSetPropSuccess"]);
			}
			return 1;
		}
		else if (strLowerMessage.substr(intMsgCnt, 2) == "ti")
		{
			string strAns = strNickName + "的疯狂发作-临时症状:\n";
			TempInsane(strAns);
			reply(strAns);
			return 1;
		}
		else if (strLowerMessage[intMsgCnt] == 'w')
		{
			intMsgCnt++;
			bool boolDetail = false;
			if (strLowerMessage[intMsgCnt] == 'w')
			{
				intMsgCnt++;
				boolDetail = true;
			}
			bool isHidden = false;
			if (strLowerMessage[intMsgCnt] == 'h')
			{
				isHidden = true;
				intMsgCnt += 1;
			}
			if (intT == 0)isHidden = false;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;

			int intTmpMsgCnt;
			for (intTmpMsgCnt = intMsgCnt; intTmpMsgCnt != strMsg.length() && strMsg[intTmpMsgCnt] != ' ';
				intTmpMsgCnt++)
			{
				if (!isdigit(static_cast<unsigned char>(strLowerMessage[intTmpMsgCnt])) && strLowerMessage[intTmpMsgCnt] != 'd' && strLowerMessage[
					intTmpMsgCnt] != 'k' && strLowerMessage[intTmpMsgCnt] != 'p' && strLowerMessage[intTmpMsgCnt] != 'b'
						&& strLowerMessage[intTmpMsgCnt] != 'a'
						&& strLowerMessage[intTmpMsgCnt] != 'f' && strLowerMessage[intTmpMsgCnt] != '+' && strLowerMessage[
							intTmpMsgCnt] != '-' && strLowerMessage[intTmpMsgCnt] != '#'
						&& strLowerMessage[intTmpMsgCnt] != 'x' && strLowerMessage[intTmpMsgCnt] != '*')
				{
					break;
				}
			}
			string strMainDice = strLowerMessage.substr(intMsgCnt, intTmpMsgCnt - intMsgCnt);
			while (isspace(static_cast<unsigned char>(strMsg[intTmpMsgCnt])))
				intTmpMsgCnt++;
			string strReason = strMsg.substr(intTmpMsgCnt);


			int intTurnCnt = 1;
			if (strMainDice.find("#") != string::npos)
			{
				string strTurnCnt = strMainDice.substr(0, strMainDice.find("#"));
				if (strTurnCnt.empty())
					strTurnCnt = "1";
				strMainDice = strMainDice.substr(strMainDice.find("#") + 1);
				const int intDefaultDice = DefaultDice.count(fromQQ) ? DefaultDice[fromQQ] : 100;
				RD rdTurnCnt(strTurnCnt, intDefaultDice);
				const int intRdTurnCntRes = rdTurnCnt.Roll();
				if (intRdTurnCntRes != 0)
				{
					if (intRdTurnCntRes == Value_Err)
					{
						reply(GlobalMsg["strValueErr"]);
						return 1;
					}
					if (intRdTurnCntRes == Input_Err)
					{
						reply(GlobalMsg["strInputErr"]);
						return 1;
					}
					if (intRdTurnCntRes == ZeroDice_Err)
					{
						reply(GlobalMsg["strZeroDiceErr"]);
						return 1;
					}
					if (intRdTurnCntRes == ZeroType_Err)
					{
						reply(GlobalMsg["strZeroTypeErr"]);
						return 1;
					}
					if (intRdTurnCntRes == DiceTooBig_Err)
					{
						reply(GlobalMsg["strDiceTooBigErr"]);
						return 1;
					}
					if (intRdTurnCntRes == TypeTooBig_Err)
					{
						reply(GlobalMsg["strTypeTooBigErr"]);
						return 1;
					}
					if (intRdTurnCntRes == AddDiceVal_Err)
					{
						reply(GlobalMsg["strAddDiceValErr"]);
						return 1;
					}
					reply(GlobalMsg["strUnknownErr"]);
					return 1;
				}
				if (rdTurnCnt.intTotal > 10)
				{
					reply(GlobalMsg["strRollTimeExceeded"]);
					return 1;
				}
				if (rdTurnCnt.intTotal <= 0)
				{
					reply(GlobalMsg["strRollTimeErr"]);
					return 1;
				}
				intTurnCnt = rdTurnCnt.intTotal;
				if (strTurnCnt.find("d") != string::npos)
				{
					string strTurnNotice = strNickName + "的掷骰轮数: " + rdTurnCnt.FormShortString() + "轮";
					if (!isHidden || intT == PrivateT)
					{
						reply(strTurnNotice);
					}
					else
					{
						strTurnNotice = "在" + printChat(fromChat) + "中 " + strTurnNotice;
						AddMsgToQueue(strTurnNotice, fromQQ, Private);
						const auto range = ObserveGroup.equal_range(fromGroup);
						for (auto it = range.first; it != range.second; ++it)
						{
							if (it->second != fromQQ)
							{
								AddMsgToQueue(strTurnNotice, it->second, Private);
							}
						}
					}
				}
			}
			if (strMainDice.empty())
			{
				reply(GlobalMsg["strEmptyWWDiceErr"]);
				return 1;
			}
			string strFirstDice = strMainDice.substr(0, strMainDice.find('+') < strMainDice.find('-')
				? strMainDice.find('+')
				: strMainDice.find('-'));
			strFirstDice = strFirstDice.substr(0, strFirstDice.find('x') < strFirstDice.find('*')
				? strFirstDice.find('x')
				: strFirstDice.find('*'));
			bool boolAdda10 = true;
			for (auto i : strFirstDice)
			{
				if (!isdigit(static_cast<unsigned char>(i)))
				{
					boolAdda10 = false;
					break;
				}
			}
			if (boolAdda10)
				strMainDice.insert(strFirstDice.length(), "a10");
			const int intDefaultDice = DefaultDice.count(fromQQ) ? DefaultDice[fromQQ] : 100;
			RD rdMainDice(strMainDice, intDefaultDice);

			const int intFirstTimeRes = rdMainDice.Roll();
			if (intFirstTimeRes != 0) {
				if (intFirstTimeRes == Value_Err)
				{
					reply(GlobalMsg["strValueErr"]);
					return 1;
				}
				if (intFirstTimeRes == Input_Err)
				{
					reply(GlobalMsg["strInputErr"]);
					return 1;
				}
				if (intFirstTimeRes == ZeroDice_Err)
				{
					reply(GlobalMsg["strZeroDiceErr"]);
					return 1;
				}
				if (intFirstTimeRes == ZeroType_Err)
				{
					reply(GlobalMsg["strZeroTypeErr"]);
					return 1;
				}
				if (intFirstTimeRes == DiceTooBig_Err)
				{
					reply(GlobalMsg["strDiceTooBigErr"]);
					return 1;
				}
				if (intFirstTimeRes == TypeTooBig_Err)
				{
					reply(GlobalMsg["strTypeTooBigErr"]);
					return 1;
				}
				if (intFirstTimeRes == AddDiceVal_Err)
				{
					reply(GlobalMsg["strAddDiceValErr"]);
					return 1;
				}
				if (intFirstTimeRes != 0)
				{
					reply(GlobalMsg["strUnknownErr"]);
					return 1;
				}
			}
			if (!boolDetail && intTurnCnt != 1)
			{
				string strAns = format(GlobalMsg["strRollTurnDice"], { strNickName ,to_string(intTurnCnt) ,rdMainDice.strDice }) + ": { ";
				if (!strReason.empty())
					strAns = format(GlobalMsg["strRollTurnDiceReason"], { strNickName ,to_string(intTurnCnt) ,rdMainDice.strDice ,strReason }) + ": { ";
				vector<int> vintExVal;
				while (intTurnCnt--)
				{
					// 此处返回值无用
					// ReSharper disable once CppExpressionWithoutSideEffects
					rdMainDice.Roll();
					strAns += to_string(rdMainDice.intTotal);
					if (intTurnCnt != 0)
						strAns += ",";
					if ((rdMainDice.strDice == "D100" || rdMainDice.strDice == "1D100") && (rdMainDice.intTotal <= 5 ||
						rdMainDice.intTotal >= 96))
						vintExVal.push_back(rdMainDice.intTotal);
				}
				strAns += " }";
				if (!vintExVal.empty())
				{
					strAns += ",极值: ";
					for (auto it = vintExVal.cbegin(); it != vintExVal.cend(); ++it)
					{
						strAns += to_string(*it);
						if (it != vintExVal.cend() - 1)
							strAns += ",";
					}
				}
				if (!isHidden || intT == PrivateT)
				{
					reply(strAns);
				}
				else
				{
					strAns = "在" + printChat(fromChat) + "中 " + strAns;
					AddMsgToQueue(strAns, fromQQ, Private);
					const auto range = ObserveGroup.equal_range(fromGroup);
					for (auto it = range.first; it != range.second; ++it)
					{
						if (it->second != fromQQ)
						{
							AddMsgToQueue(strAns, it->second, Private);
						}
					}
				}
			}
			else
			{
				while (intTurnCnt--)
				{
					// 此处返回值无用
					// ReSharper disable once CppExpressionWithoutSideEffects
					rdMainDice.Roll();
					string strDiceRes = boolDetail ? rdMainDice.FormCompleteString() : rdMainDice.FormShortString();
					string strAns = format(GlobalMsg["strRollDice"], { strNickName ,strDiceRes });
					if (!strReason.empty())
						strAns = format(GlobalMsg["strRollDiceReason"], { strNickName ,strDiceRes ,strReason });
					if (!isHidden || intT == PrivateT)
					{
						reply(strAns);
					}
					else
					{
						strAns = "在" + printChat(fromChat) + "中 " + strAns;
						AddMsgToQueue(strAns, fromQQ, Private);
						const auto range = ObserveGroup.equal_range(fromGroup);
						for (auto it = range.first; it != range.second; ++it)
						{
							if (it->second != fromQQ)
							{
								AddMsgToQueue(strAns, it->second, Private);
							}
						}
					}
				}
			}
			if (isHidden)
			{
				const string strReply = format(GlobalMsg["strRollHidden"], { strNickName });
				reply(strReply);
			}
			return 1;
		}
		else if (strLowerMessage[intMsgCnt] == 'r' || strLowerMessage[intMsgCnt] == 'o' || strLowerMessage[intMsgCnt] == 'h'
			|| strLowerMessage[intMsgCnt] == 'd')
		{
			bool isHidden = false;
			if (strLowerMessage[intMsgCnt] == 'h')
				isHidden = true;
			intMsgCnt += 1;
			bool boolDetail = true;
			if (strMsg[intMsgCnt] == 's')
			{
				boolDetail = false;
				intMsgCnt++;
			}
			if (strLowerMessage[intMsgCnt] == 'h')
			{
				isHidden = true;
				intMsgCnt += 1;
			}
			if (intT == 0)isHidden = false;
			while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])))
				intMsgCnt++;
			string strMainDice;
			string strReason;
			bool tmpContainD = false;
			int intTmpMsgCnt;
			for (intTmpMsgCnt = intMsgCnt; intTmpMsgCnt != strMsg.length() && strMsg[intTmpMsgCnt] != ' ';
				intTmpMsgCnt++)
			{
				if (strLowerMessage[intTmpMsgCnt] == 'd' || strLowerMessage[intTmpMsgCnt] == 'p' || strLowerMessage[
					intTmpMsgCnt] == 'b' || strLowerMessage[intTmpMsgCnt] == '#' || strLowerMessage[intTmpMsgCnt] == 'f'
						||
						strLowerMessage[intTmpMsgCnt] == 'a')
					tmpContainD = true;
					if (!isdigit(static_cast<unsigned char>(strLowerMessage[intTmpMsgCnt])) && strLowerMessage[intTmpMsgCnt] != 'd' && strLowerMessage[
						intTmpMsgCnt] != 'k' && strLowerMessage[intTmpMsgCnt] != 'p' && strLowerMessage[intTmpMsgCnt] != 'b'
							&&
							strLowerMessage[intTmpMsgCnt] != 'f' && strLowerMessage[intTmpMsgCnt] != '+' && strLowerMessage[
								intTmpMsgCnt
							] != '-' && strLowerMessage[intTmpMsgCnt] != '#' && strLowerMessage[intTmpMsgCnt] != 'a'&& strLowerMessage[intTmpMsgCnt] != 'x'&& strLowerMessage[intTmpMsgCnt] != '*')
					{
						break;
					}
			}
			if (tmpContainD)
			{
				strMainDice = strLowerMessage.substr(intMsgCnt, intTmpMsgCnt - intMsgCnt);
				while (isspace(static_cast<unsigned char>(strMsg[intTmpMsgCnt])))
					intTmpMsgCnt++;
				strReason = strMsg.substr(intTmpMsgCnt);
			}
			else
				strReason = strMsg.substr(intMsgCnt);

			int intTurnCnt = 1;
			if (strMainDice.find("#") != string::npos)
			{
				string strTurnCnt = strMainDice.substr(0, strMainDice.find("#"));
				if (strTurnCnt.empty())
					strTurnCnt = "1";
				strMainDice = strMainDice.substr(strMainDice.find("#") + 1);
				const int intDefaultDice = DefaultDice.count(fromQQ) ? DefaultDice[fromQQ] : 100;
				RD rdTurnCnt(strTurnCnt, intDefaultDice);
				const int intRdTurnCntRes = rdTurnCnt.Roll();
				if (intRdTurnCntRes == Value_Err)
				{
					reply(GlobalMsg["strValueErr"]);
					return 1;
				}
				if (intRdTurnCntRes == Input_Err)
				{
					reply(GlobalMsg["strInputErr"]);
					return 1;
				}
				if (intRdTurnCntRes == ZeroDice_Err)
				{
					reply(GlobalMsg["strZeroDiceErr"]);
					return 1;
				}
				if (intRdTurnCntRes == ZeroType_Err)
				{
					reply(GlobalMsg["strZeroTypeErr"]);
					return 1;
				}
				if (intRdTurnCntRes == DiceTooBig_Err)
				{
					reply(GlobalMsg["strDiceTooBigErr"]);
					return 1;
				}
				if (intRdTurnCntRes == TypeTooBig_Err)
				{
					reply(GlobalMsg["strTypeTooBigErr"]);
					return 1;
				}
				if (intRdTurnCntRes == AddDiceVal_Err)
				{
					reply(GlobalMsg["strAddDiceValErr"]);
					return 1;
				}
				if (intRdTurnCntRes != 0)
				{
					reply(GlobalMsg["strUnknownErr"]);
					return 1;
				}
				if (rdTurnCnt.intTotal > 10)
				{
					reply(GlobalMsg["strRollTimeExceeded"]);
					return 1;
				}
				if (rdTurnCnt.intTotal <= 0)
				{
					reply(GlobalMsg["strRollTimeErr"]);
					return 1;
				}
				intTurnCnt = rdTurnCnt.intTotal;
				if (strTurnCnt.find("d") != string::npos)
				{
					string strTurnNotice = format(GlobalMsg["strRollTurn"], { strNickName,rdTurnCnt.FormShortString() });
					if (!isHidden)
					{
						reply(strTurnNotice);
					}
					else
					{
						strTurnNotice = "在" + printChat(fromChat) + "中 " + strTurnNotice;
						AddMsgToQueue(strTurnNotice, fromQQ, Private);
						const auto range = ObserveGroup.equal_range(fromGroup);
						for (auto it = range.first; it != range.second; ++it)
						{
							if (it->second != fromQQ)
							{
								AddMsgToQueue(strTurnNotice, it->second, Private);
							}
						}
					}
				}
			}
			const int intDefaultDice = DefaultDice.count(fromQQ) ? DefaultDice[fromQQ] : 100;
			RD rdMainDice(strMainDice, intDefaultDice);

			const int intFirstTimeRes = rdMainDice.Roll();
			if (intFirstTimeRes == Value_Err)
			{
				reply(GlobalMsg["strValueErr"]);
				return 1;
			}
			if (intFirstTimeRes == Input_Err)
			{
				reply(GlobalMsg["strInputErr"]);
				return 1;
			}
			if (intFirstTimeRes == ZeroDice_Err)
			{
				reply(GlobalMsg["strZeroDiceErr"]);
				return 1;
			}
			if (intFirstTimeRes == ZeroType_Err)
			{
				reply(GlobalMsg["strZeroTypeErr"]);
				return 1;
			}
			if (intFirstTimeRes == DiceTooBig_Err)
			{
				reply(GlobalMsg["strDiceTooBigErr"]);
				return 1;
			}
			if (intFirstTimeRes == TypeTooBig_Err)
			{
				reply(GlobalMsg["strTypeTooBigErr"]);
				return 1;
			}
			if (intFirstTimeRes == AddDiceVal_Err)
			{
				reply(GlobalMsg["strAddDiceValErr"]);
				return 1;
			}
			if (intFirstTimeRes != 0)
			{
				reply(GlobalMsg["strUnknownErr"]);
				return 1;
			}
			if (!boolDetail && intTurnCnt != 1)
			{
				string strAns = format(GlobalMsg["strRollTurnDice"], { strNickName ,to_string(intTurnCnt) ,rdMainDice.strDice }) + ": { ";
				if (!strReason.empty())
					strAns = format(GlobalMsg["strRollTurnDiceReason"], { strNickName ,to_string(intTurnCnt) ,rdMainDice.strDice ,strReason}) + ": { ";
				vector<int> vintExVal;
				while (intTurnCnt--)
				{
					// 此处返回值无用
					// ReSharper disable once CppExpressionWithoutSideEffects
					rdMainDice.Roll();
					strAns += to_string(rdMainDice.intTotal);
					if (intTurnCnt != 0)
						strAns += ",";
					if ((rdMainDice.strDice == "D100" || rdMainDice.strDice == "1D100") && (rdMainDice.intTotal <= 5 ||
						rdMainDice.intTotal >= 96))
						vintExVal.push_back(rdMainDice.intTotal);
				}
				strAns += " }";
				if (!vintExVal.empty())
				{
					strAns += ",极值: ";
					for (auto it = vintExVal.cbegin(); it != vintExVal.cend(); ++it)
					{
						strAns += to_string(*it);
						if (it != vintExVal.cend() - 1)
							strAns += ",";
					}
				}
				if (!isHidden)
				{
					reply(strAns);
				}
				else
				{
					strAns = "在" + printChat(fromChat) + "中 " + strAns;
					AddMsgToQueue(strAns, fromQQ, Private);
					const auto range = ObserveGroup.equal_range(fromGroup);
					for (auto it = range.first; it != range.second; ++it)
					{
						if (it->second != fromQQ)
						{
							AddMsgToQueue(strAns, it->second, Private);
						}
					}
				}
			}
			else
			{
				string strDiceRes;
				if (intTurnCnt == 1)strDiceRes = boolDetail ? rdMainDice.FormCompleteString() : rdMainDice.FormShortString();
				else while (intTurnCnt--)
				{
					// 此处返回值无用
					// ReSharper disable once CppExpressionWithoutSideEffects
					rdMainDice.Roll();
					strDiceRes += "\n" + (boolDetail ? rdMainDice.FormCompleteString() : rdMainDice.FormShortString());
				}
					string strAns = format(GlobalMsg["strRollDice"], { strNickName ,strDiceRes });
					if (!strReason.empty())
						strAns = format(GlobalMsg["strRollDiceReason"], { strNickName ,strDiceRes ,strReason});
					if (!isHidden)
					{
						reply(strAns);
					}
					else
					{
						strAns = "在" + printChat(fromChat) + "中 " + strAns;
						AddMsgToQueue(strAns, fromQQ, Private);
						const auto range = ObserveGroup.equal_range(fromGroup);
						for (auto it = range.first; it != range.second; ++it)
						{
							if (it->second != fromQQ)
							{
								AddMsgToQueue(strAns, it->second, Private);
							}
						}
					}
			}
			if (isHidden)
			{
				const string strReply = format(GlobalMsg["strRollHidden"], { strNickName });
				reply(strReply);
			}
			return 1;
		}
		return 0;
	}
	int CustomReply(){
		string strKey = readRest();
		if (CardDeck::mReplyDeck.count(strKey)) {
			reply(CardDeck::drawCard(CardDeck::mReplyDeck[strKey], true));
			AddFrq(fromQQ, fromTime, fromChat);
			return 1;
		}
		else return 0;
	}
	//判断是否响应
	bool DiceFilter() {
		init(strMsg);
		while (isspace(static_cast<unsigned char>(strMsg[0])))
			strMsg.erase(strMsg.begin());
		bool isOtherCalled = false;
		string strAt = "[CQ:at,qq=" + to_string(getLoginQQ()) + "]";
		while (strMsg.substr(0, 6) == "[CQ:at")
		{
			if (strMsg.substr(0, strAt.length()) == strAt)
			{
				strMsg = strMsg.substr(strAt.length());
				isCalled = true;
			}
			else if (strMsg.substr(0, 14) == "[CQ:at,qq=all]") {
				strMsg = strMsg.substr(14);
				isCalled = true;
			}
			else if (strMsg.find("]") != string::npos)
			{
				strMsg = strMsg.substr(strMsg.find("]") + 1);
				isOtherCalled = true;
			}
			while (isspace(static_cast<unsigned char>(strMsg[0])))
				strMsg.erase(strMsg.begin());
		}
		if (isOtherCalled && !isCalled)return false;
		init2(strMsg);
		if (fromType == Private) isCalled = true;
		isMaster = fromQQ == masterQQ && boolMasterMode;
		isAdmin = isMaster || AdminQQ.count(fromQQ);
		isBotOff = (boolConsole["DisabledGlobal"] && (!isAdmin || !isCalled)) || (!isCalled && (fromType == Group && DisabledGroup.count(fromGroup) || fromType == Discuss && DisabledDiscuss.count(fromGroup)));
		if (DiceReply()) {
			AddFrq(fromQQ, fromTime, fromChat);
			return 1;
		}
		else if(isBotOff)return boolConsole["DisabledBlock"];
		else return CustomReply();
	}

private:
	int intMsgCnt = 0;
	bool isBotOff = false;
	bool isCalled = false;
	bool isMaster = false;
	bool isAdmin = false;
	bool isAuth = false;
	bool isLinkOrder = false;
	//跳过空格
	void readSkipSpace() {
		while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))intMsgCnt++;
	}
	string readUntilSpace() {
		string strPara;
		readSkipSpace(); 
		while (!isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && intMsgCnt != strLowerMessage.length()) {
			strPara += strMsg[intMsgCnt];
			intMsgCnt++;
		}
		return strPara;
	}
	string readRest() {
		readSkipSpace();
		return strMsg.substr(intMsgCnt);
	}
	//读取参数(统一小写)
	string readPara() {
		string strPara;
		while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))intMsgCnt++;
		while (!isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && !isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt])) && (strLowerMessage[intMsgCnt] != '-') && (strLowerMessage[intMsgCnt] != '+') && intMsgCnt != strLowerMessage.length()) {
			strPara += strLowerMessage[intMsgCnt];
			intMsgCnt++;
		}
		return strPara;
	}
	//读取数字
	string readDigit() {
		string strMum;
		while (!isdigit(static_cast<unsigned char>(strMsg[intMsgCnt])) && intMsgCnt != strMsg.length())intMsgCnt++;
		while (isdigit(static_cast<unsigned char>(strMsg[intMsgCnt]))) {
			strMum += strMsg[intMsgCnt];
			intMsgCnt++;
		}
		if (strMsg[intMsgCnt] == ']')intMsgCnt++;
		return strMum;
	}
	//读取群号
	long long readID() {
		string strGroup = readDigit();
		if (strGroup.empty()) return 0;
		return stoll(strGroup);
	}
	//是否可看做掷骰表达式
	bool isRollDice() {
		readSkipSpace();
		if (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt]))
			|| strLowerMessage[intMsgCnt] == 'd' || strLowerMessage[intMsgCnt] == 'k'
			|| strLowerMessage[intMsgCnt] == 'p' || strLowerMessage[intMsgCnt] == 'b'
			|| strLowerMessage[intMsgCnt] == 'f'
			|| strLowerMessage[intMsgCnt] == '+' || strLowerMessage[intMsgCnt] == '-'
			|| strLowerMessage[intMsgCnt] == 'a'
			|| strLowerMessage[intMsgCnt] == 'x' || strLowerMessage[intMsgCnt] == '*') {
			return true;
		}
		else return false;
	}
	//读取掷骰表达式
	string readDice(){
		string strDice;
		while (isspace(static_cast<unsigned char>(strLowerMessage[intMsgCnt])))intMsgCnt++;
		while (isdigit(static_cast<unsigned char>(strLowerMessage[intMsgCnt]))
			|| strLowerMessage[intMsgCnt] == 'd' || strLowerMessage[intMsgCnt] == 'k'
			|| strLowerMessage[intMsgCnt] == 'p' || strLowerMessage[intMsgCnt] == 'b'
			|| strLowerMessage[intMsgCnt] == 'f'
			|| strLowerMessage[intMsgCnt] == '+' || strLowerMessage[intMsgCnt] == '-'
			|| strLowerMessage[intMsgCnt] == 'a'
			|| strLowerMessage[intMsgCnt] == 'x' || strLowerMessage[intMsgCnt] == '*')
		{
			strDice += strMsg[intMsgCnt];
			intMsgCnt++;
		}
		return strDice;
	}
	//
	int readChat(chatType &ct) {
		string strT = readPara();
		msgtype T = Private;
		long long llID = readID();
		if (strT == "qq") {
			T = Private;
		}
		else if (strT == "group"){
			T = Group;
		}
		else if (strT == "discuss") {
			T = Discuss;
		}
		else return -1;
		if (llID) {
			ct = { llID,T };
			return 0;
		}
		else return -2;
	}
	//读取分项
	string readItem() {
		string strMum;
		while (isspace(static_cast<unsigned char>(strMsg[intMsgCnt])) || strMsg[intMsgCnt] == '|')intMsgCnt++;
		while (strMsg[intMsgCnt] != '|'&& intMsgCnt != strMsg.length()) {
			strMum += strMsg[intMsgCnt];
			intMsgCnt++;
		}
		return strMum;
	}
};

#endif /*DICE_EVENT*/
