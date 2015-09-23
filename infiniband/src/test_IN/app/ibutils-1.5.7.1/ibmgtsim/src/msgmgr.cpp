/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#include "msgmgr.h"
#include <string>
#include <stdio.h>
#include <string.h>

using namespace std;


//
// MESSAGE TYPE CLASS
//
msgType::msgType(char s, string &fmt, string ctx, string mod)
{
    severity    = s;
    format      = fmt;
    context     = ctx;
    module      = mod;

    // calc the number of fields
    int pos = 0;
    numFields = 0;
    while ((pos = 1 + format.find('$', pos)) > 0)
        ++numFields;

    // limit to 6 :
    if (numFields > 6) {
        cerr << "-E- msgManager too many fields (>6) in msgType:" << fmt << endl;
        numFields = 6;
    }
}


//
// MESSAGE MANAGER CLASS
//
string msgManager::msg2string(msgObj msg,
        int externalVerbosity,          // if not 0 will be used as the verbosity
        int errFatals)                  // if not 0 will cerr any  FATAL err
{
    // find the message format:
    int_mtype_map::const_iterator mTypeI = types.find(msg.typeId);
    if (mTypeI == types.end()) {
        cerr << "-E- Fail to find message type by id:" << msg.typeId << endl;
        return "";
    }

    msgType t = (*mTypeI).second;

    int vl;
    if (externalVerbosity == 0)
        vl = getVerbLevel(t.module);
    else
        vl = externalVerbosity;

    // filter by verbose level:
    if ( (t.severity == 'F' && (vl & MsgShowFatal)) ||
            (t.severity == 'E' && (vl & MsgShowError)) ||
            (t.severity == 'W' && (vl & MsgShowWarning)) ||
            (t.severity == 'I' && (vl & MsgShowInfo)) ||
            (t.severity == 'M' && (vl & MsgShowMads)) ||
            (t.severity == 'R' && (vl & MsgShowFrames)) ||
            (t.severity == 'V' && (vl & MsgShowVerbose))) {
        char res[1024];
        char tmp[1024];

        res[0] = '\0';

        // add time if required
        if (vl & MsgShowTime)
            sprintf(res, "[%09ld:%09ld]", msg.when.tv_sec, (long)msg.when.tv_usec);

        // start with severity
        sprintf(tmp, "-%c-", t.severity);
        strcat(res, tmp);

        // include parenthesis if either context or source
        if ((vl & MsgShowContext) || (vl & MsgShowSource) || (vl & MsgShowModule) )
            strcat(res,"(");

        // append the module if required:
        if (vl & MsgShowModule) {
            strcat(res, t.module.c_str());
            strcat(res, " ");
        }

        // append the context if required:
        if (vl & MsgShowContext)
            strcat(res, t.context.c_str());

        // append file and line if required:
        if (vl & MsgShowSource) {
            sprintf(tmp, " %s,%d", msg.inFile.c_str(), msg.lineNum);
            strcat(res, tmp);
        }

        // close the parent system or space
        if ((vl & MsgShowContext) || (vl & MsgShowSource) || (vl & MsgShowModule))
            strcat(res,") ");
        else
            strcat(res," ");

        // go over the format:
        int pos = 0, nextPos = 0;
        int numFields = 0;
        while ((nextPos = t.format.find('$', pos)) >= 0) {
            strcat(res,t.format.substr(pos,nextPos - pos).c_str());
            switch (++numFields) {
            case 1: strcat(res, msg.f1.c_str()); break;
            case 2: strcat(res, msg.f2.c_str()); break;
            case 3: strcat(res, msg.f3.c_str()); break;
            case 4: strcat(res, msg.f4.c_str()); break;
            case 5: strcat(res, msg.f5.c_str()); break;
            case 6: strcat(res, msg.f6.c_str()); break;
            }
            pos = nextPos + 1;
        }

        strcat(res, t.format.substr(pos).c_str());
        strcat(res, "\n");

        // send FATAL errors directly to the stderr
        if (errFatals && (t.severity == 'F') && (vl & MsgShowFatal))
            cerr << res;
        return res;
    }
    return "";
}

// get number of outstanding messages of the given severity
int msgManager::outstandingMsgCount(int vl)
{
    // go over all messages past the pendingMsgsI iterator and count them if
    // match the given verbose level
    int cnt = 0;

    unsigned int I = pendingMsgsI;
    msgType t;
    pthread_mutex_lock(&lock);
    while (I < log.size()) {
        // get the message type obj:
        t = types[(log[I]).typeId];
        if ( (t.severity == 'F' && (vl & MsgShowFatal)) ||
                (t.severity == 'E' && (vl & MsgShowError)) ||
                (t.severity == 'W' && (vl & MsgShowWarning)) ||
                (t.severity == 'I' && (vl & MsgShowInfo)) ||
                (t.severity == 'M' && (vl & MsgShowMads)) ||
                (t.severity == 'R' && (vl & MsgShowFrames)) ||
                (t.severity == 'V' && (vl & MsgShowVerbose)) )
            ++cnt;
        ++I;
    }
    pthread_mutex_unlock(&lock);
    return cnt;
}

// get outstanding messages of the given severity
string msgManager::outstandingMsgs(int vl)
{
    // go over all messages past the pendingMsgsI iterator and collect
    // match the given verbose level
    string res = "";

    pthread_mutex_lock(&lock);
    unsigned int I = pendingMsgsI;

    msgType t;
    while (I < log.size()) {
        // get the message type obj:
        t = types[(log[I]).typeId];
        if ( (t.severity == 'F' && (vl & MsgShowFatal)) ||
                (t.severity == 'E' && (vl & MsgShowError)) ||
                (t.severity == 'W' && (vl & MsgShowWarning)) ||
                (t.severity == 'I' && (vl & MsgShowInfo)) ||
                (t.severity == 'M' && (vl & MsgShowMads)) ||
                (t.severity == 'R' && (vl & MsgShowFrames)) ||
                (t.severity == 'V' && (vl & MsgShowVerbose)) )
            res += msg2string(log[I], vl);
        I++;
    }
    pthread_mutex_unlock(&lock);
    return res;
}

// return the next message string if included in the given types
string msgManager::getNextMessage()
{
    string res("");

    pthread_mutex_lock(&lock);

    if (pendingMsgsI == log.size() - 1)
        res = msg2string(log[pendingMsgsI++]);

    pthread_mutex_unlock(&lock);
    return res;
}

void  msgManager::nullOutstandingMsgs()
{
    pthread_mutex_lock(&lock);

    if (log.size() == 0)
        pendingMsgsI = 0;
    else
        pendingMsgsI = log.size();

    pthread_mutex_unlock(&lock);
}

// declare a new message type - return the generator id:
int msgManager::reg(char s, string fmt, string ctx, string mod)
{
    msgType t(s,fmt,ctx,mod);   // create a msg type
    pthread_mutex_lock(&lock);
    int id = types.size() + 1;
    types[id] = t;              // store in map
    pthread_mutex_unlock(&lock);
    return id;
}

// get a new message and do something with it
int msgManager::send(int typeId, string fn, int ln,
        msgStr i1, msgStr i2, msgStr i3, msgStr i4, msgStr i5, msgStr i6)
{
    // create a new message
    msgObj msg(typeId, i1.s,i2.s,i3.s,i4.s,i5.s,i6.s,fn,ln);

    // store in the log
    pthread_mutex_lock(&lock);

    // We store all messages in the log vector - but we can void that for now
    // log.push_back(msg);

    // if the message if Fatal send it to stderr if we are logging to file.
    int sendFatalsToCerr = 0;
    if ((*outStreamP != cout) && (*outStreamP != cerr))
        sendFatalsToCerr = 1;

    // handle the new message
    if (outStreamP) {
        // default verbosity, err on fatal
        (*outStreamP) << msg2string(msg, 0, sendFatalsToCerr);
        outStreamP->flush();
    }

    // if we unlock after we stream we serialize the display
    pthread_mutex_unlock(&lock);

    return 0;
}

msgStr::msgStr(const char cp[])             {if (cp) {s = string(cp);};}
msgStr::msgStr(const int i)                 {char b[8];  sprintf(b, "%d", i); s = string(b);}
msgStr::msgStr(const long int i)            {char b[8];  sprintf(b, "%ld", i);s = string(b);}
msgStr::msgStr(const float i)               {char b[16]; sprintf(b, "%f", i); s = string(b);}
msgStr::msgStr(const double i)              {char b[16]; sprintf(b, "%f", i); s = string(b);}
msgStr::msgStr(const unsigned long int i)   {char b[16]; sprintf(b, "%lu", i);s = string(b);}
msgStr::msgStr(const unsigned int i)        {char b[8];  sprintf(b, "%u", i); s = string(b);}
msgStr::msgStr(const unsigned short i)      {char b[8];  sprintf(b, "%u", i); s = string(b);}
msgStr::msgStr(const unsigned long long i)  {char b[20];  sprintf(b, "0x%016llx", i); s = string(b);}


msgManager &msgMgr(int vl, std::ostream *o)
{
    static msgManager *pMgr = NULL;
    if (!pMgr)
        pMgr = new msgManager(vl, o);
    return (*pMgr);
};


// we provide two global definitions for entering and leaving functions:
int msgMgrEnterFunc = msgMgr().reg('R',"$ [", "top", "msg");
int msgMgrLeaveFunc = msgMgr().reg('R',"$ ]", "top", "msg");


#ifdef MSG_MGR_TEST


int main(int argc, string **argv)
{
    msgMgr( MsgShowFatal | MsgShowError | MsgShowWarning, &cerr);
    MSGREG(err1,
            'E',  // severity
            "Fail to find node:$ for inst:$ of type:$",
            "noModule");
    MSGREG(verb1,'V', "This verbose with param:$ bla","");
    msgMgr().send(err1,__FILE__,__LINE__,"n18","i1","output");
    MSGSND(verb1,"pv1");
    MSGSND(err1,"node","inst","type");
    static int mod1Err = msgMgr().reg('V', "This is module param:$", "main", "myModule");
    MSGREG(mod2Err,'V', "This is another module param:$", "otherModule");

    msgMgr().setVerbLevel(MsgShowAll, "myModule");
    msgMgr().send(mod1Err,__FILE__,__LINE__,"PARAM1");
    MSGSND(mod2Err,"PARAM2");
    MSGREG(err3, 'E', "This is error no param", "mySecondModule");
    MSGSND(err3);

    cout << "Getting all messages..." <<endl;
    msgMgr().clrVerbLevel("myModule");
    msgMgr().send(mod1Err,__FILE__,__LINE__,"PARAM1");

    cout << msgMgr().outstandingMsgs(MsgShowAll);
}


#endif

