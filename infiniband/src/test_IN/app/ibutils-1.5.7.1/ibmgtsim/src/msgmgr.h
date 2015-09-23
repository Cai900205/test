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


///////////////////////////////////////////////////////////////////////////
//
// Message Manager -
//
// A centralized way for error warning info verbose etc messaging.
//
// The main idea:
// 1. An application register each new message type (providing a module name, and a
//    parameterized msg string) using msgMan.reg which returns the msgType object
// 2. An application then uses msgType.msg method to send the message providing
//    only the message arguments.
//
// EXAMPLE CODE:
//
// 0. The following flags define the verbosity level:
//    MsgShowFatal   - show fatal errors
//    MsgShowError   - show algorithmic or data non fatal errors
//    MsgShowWarning - show warnings
//    MsgShowInfo    - show info massages
//    MsgShowVerbose - show verbose messages
//    MsgShowMads    - show MAD messages contents
//    MsgShowContext - show the context info for the message (normally the function name)
//    MsgShowSource  - show the file name and line of the message
//    MsgShowAll     - all the above
//    MsgDefault     - MsgShowContext | MsgShowFatal | MsgShowError | MsgShowWarning | MsgShowInfo
//
// 1. Get a ref to the massage manager
//    NOTE: on the first call it's out stream and verbosity is defined.
//    msgMgr(MsgDefault, &cout)
//
// 2. Define some message templates:
//
//    static int errMsg1 = msgMgr().reg(
//     'E',                                        // severity
//     "Fail to find node:$ for inst:$ of type:$", // actual format
//     "myProcedure"                               // context
//     "myModule"                                  // module name
//     );
//
// 3. Send a message (using the predefined type errMsg1):
//    msgMgr().send(errMsg1,__FILE__,__LINE__,"n18","i1","output");
//
// 4. You can control the verbosity of each module:
//    msgMgr().setVerbLevel(MsgShowAll, "myModule");
//    NOTE: Even if the global verbosity is set to errors only a verbose message
//          of type with module "myModule" will be shown.
//
//    For simplicity we provide macro versions:
//
//    MSGREG(varName, verbosity, msgTemplate, module) - register new type for current module
//
//    MSGSND(varName, "parameter1", "parameter2", ...) - send the message
//
//    The example above will look like:
//    MSGREG(errMsg1, 'E', "Fail to find node:$ for inst:$ of type:$","myModule");
//    MSGSND(errMsg1, "n18","i1","output");
//
// HACK: currently I use max f 6 optional fields
///////////////////////////////////////////////////////////////////////////


#ifndef __MSG_MGR_H__
#define __MSG_MGR_H__

#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <list>
#include <map>


#define MSGREG(varName, verbosity, msgTemplate, module) \
        static int varName = msgMgr().reg(verbosity, msgTemplate, __FUNCTION__, module);

#define MSGGLBREG(varName, verbosity, msgTemplate, module)  \
        static int varName = msgMgr().reg(verbosity, msgTemplate, "Global", module);

#define MSGSND(varName, ...)    \
        msgMgr().send(varName ,__FILE__,__LINE__,##__VA_ARGS__);

extern int msgMgrEnterFunc;
#define MSG_ENTER_FUNC  \
        msgMgr().send(msgMgrEnterFunc ,__FILE__,__LINE__,__FUNCTION__);

extern int msgMgrLeaveFunc;
#define MSG_EXIT_FUNC   \
        msgMgr().send(msgMgrLeaveFunc ,__FILE__,__LINE__,__FUNCTION__);

#if 0
    #define MSG_ENTER_FUNC
    #define MSG_EXIT_FUNC
#endif


// we use specialization of strings to support casting of various types:
class msgStr {
private:

public:
    std::string s;
    msgStr(const msgStr &os) {s = os.s;};
    msgStr(const char cp[]);
    msgStr(const std::string str) {s = str;};
    msgStr(const int i);
    msgStr(const long int i);
    msgStr(const float i);
    msgStr(const double i);
    msgStr(const unsigned int i);
    msgStr(const unsigned long int i);
    msgStr(const unsigned short int i);
    msgStr(const unsigned long long i);

    //msgStr operator+ (const char cp[]) { s += std::string(cp);};
    msgStr operator+ (const char cp1[]) {
        msgStr tmp(*this);
        tmp.s += std::string(cp1);
        return tmp;
    }
    // msgStr operator+ (const msgStr os) { s += os.s;};
};


// a single message object
class msgObj {
private:
    // the message fields
    std::string f1,f2,f3,f4,f5,f6;
    int         lineNum;
    std::string inFile;
    int         typeId;
    struct timeval when;

 public:
    inline msgObj() {typeId = 0;};
    msgObj(int t,
            std::string i1, std::string i2, std::string i3, std::string i4, std::string i5, std::string i6,
            std::string fn = "", int ln = 0) {
        struct timezone tz; // we need it temporarily
        typeId = t; f1 = i1; f2 = i2; f3 = i3; f4 = i4; f5 = i5; f6 = i6;
        inFile = fn; lineNum = ln;
        gettimeofday( &when, &tz );
    }
    // ~msgObj();
    friend class msgManager;
};


// Each message has an associated type.The type defines the format of the message
// and the module it belongs to.
class msgType {
private:
    char severity;
    std::string format;     // format string is "<any string> $ <any string> $ ...."
    int numFields;          // number of fields in the format
    std::string context;    // the context (the function) of the message
    std::string module;     // the module name - message filtering can be done in module granularity

public:
    msgType() { severity = 'U'; numFields = 0; context = ""; module = "";};
    msgType(char s, std::string &fmt, std::string ctx = "", std::string mod = "");

    // ~msgType(); // NOTE it will not be removed from the mgr list of generators!

    // generate a msg object and send it to the manager
    // HACK WE CURRENTLY LIMIT THE NUMBER OF FIELDS TO 6
    // string send(string f1 = "", string f2 = "", string f3 = "",
    // string f4 = "", string f5 = "", string f6 = "");
    friend class msgManager;
};


#define msg_vec              std::vector<msgObj>
#define int_mtype_map        std::map<int, msgType, std::less<int> >
#define module_verbosity_map std::map< std::string, int, std::less<std::string> >

// modes of verbose
const int MsgShowFatal   = 0x01;
const int MsgShowError   = 0x02;
const int MsgShowWarning = 0x04;
const int MsgShowInfo    = 0x08;
const int MsgShowVerbose = 0x10;
const int MsgShowContext = 0x20;
const int MsgShowSource  = 0x40;
const int MsgShowTime    = 0x80;
const int MsgShowModule  = 0x100;
const int MsgShowMads    = 0x200;
const int MsgShowFrames  = 0x400;
const int MsgShowAll     = 0xffff;
const int MsgDefault     = 0x62f;

// This is the manager class
class msgManager {
private:
    msg_vec         log;            // hold all messages in generation order
    int_mtype_map   types;          // hold all generators declared
    std::ostream *  outStreamP;     // target stream
    pthread_mutex_t lock;           // need to lock the log vectors during push/pop

    // verbLevel - controls the msg verbosity.
    module_verbosity_map  verbLevel;

    unsigned int pendingMsgsI;

    // will use the module verbosity of the message type if the
    // external verbosity is not defined.
    std::string msg2string(msgObj msg, int externalVerbosity = 0,
            int errFatals = 0); // produce a message string.

public:
    // default constructor in any case ...
    msgManager() {
        verbLevel[std::string("")] = MsgDefault;
        outStreamP = & std::cout;
        pendingMsgsI = 0;
        pthread_mutex_init(&lock, NULL);
    };

    // The constructor
    msgManager(const int vl, std::ostream *o = & std::cout) {
        verbLevel[std::string("")] = vl; outStreamP = o; pendingMsgsI = 0;
        // Initialize the lock object
        pthread_mutex_init(&lock, NULL);
    };

    ~msgManager();

    // get
    inline int getVerbLevel(std::string module = "") {
        module_verbosity_map::iterator vI = verbLevel.find(module);
        if (vI != verbLevel.end()) {
            return((*vI).second);
        } else {
            return(verbLevel[std::string("")]);
        };
    };
    // clr
    inline int clrVerbLevel(std::string module = "") {
        if (module.size() == 0) return 0;
        module_verbosity_map::iterator vI = verbLevel.find(module);
        if (vI != verbLevel.end())
            verbLevel.erase(vI);
        return 0;
    };

    inline void setVerbLevel(int vl, std::string module = "") {verbLevel[module] = vl;};
    inline std::ostream *getOutStream() {return outStreamP;};
    inline void setOutStream(std::ostream * o) {outStreamP = o;};

    // get number of outstanding messages of the given severity
    int outstandingMsgCount(int vl = MsgShowFatal | MsgShowError);

    // get all outstanding messages
    std::string outstandingMsgs(int vl = MsgShowFatal | MsgShowError);

    // return the next message string
    std::string getNextMessage();

    // null the list of outstanding messages:
    void nullOutstandingMsgs();

    // declare a new message type - return the generator id:
    int reg(char s, std::string fmt, std::string ctx = "", std::string mod = "");

    // get a new message and do something with it
    int send(int typeId, std::string fn = "", int ln = 0,
            msgStr i1="",msgStr i2="",msgStr i3="",msgStr i4="",
            msgStr i5="",msgStr i6="");

    // FUTURE:
    // Other fancy utilities like summary, sorting of messages etc.
};


// we want to have a singleton massage manager
msgManager &msgMgr(int vl = MsgDefault, std::ostream *o = & std::cout);


#endif

