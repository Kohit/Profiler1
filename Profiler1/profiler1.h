// profile1.h

/**
* Profiler1 is a c++ profiler, aim to find out the time & memory cost 
* of each function call, with the help of compiler instrumentation,
* using this library doesn't need to modify your source code.
* This profiler could also use to trace the call stack, detect memory leak.
* 
* Copyright(C) 2020 kohit (kohits@outlook.com or https://github.com/Kohit)
* 
* The MIT License
*     Permission is hereby granted, free of charge, to any person obtaining a copy
*     of this software and associated documentation files (the "Software"), to deal
*     in the Software without restriction, including without limitation the rights 
*     to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
*     of the Software, and to permit persons to whom the Software is furnished 
*     to do so, subject to the following conditions:
*     The above copyright notice and this permission notice shall be included in all 
*     copies or substantial portions of the Software.
*     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
*     INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
*     PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
*     LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
*     TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
*     USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
* Usage:
* compile with cl:
* 1. simply compile profiler1 alone, to generate (obj/lib/dll) binary.
* 2. compile target source code, with /Gh /GH option, then link the binary.
* for more detail, check https://docs.microsoft.com/en-us/cpp/build
* /reference/gh-enable-penter-hook-function?view=vs-2019
* 
* Example:
* see test.cpp
**/

#pragma once
#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <Windows.h>
#include <DbgHelp.h>
#include <map>
#include <string>
#include <vector>
#include <stack>

/**
 * @brief Stack Frame，data of each function execution
 * 
 */
struct P1_StackFrame {
	unsigned id;
	DWORD64 dwAddr;			// address
	__int64 i64StartTime;
	__int64 i64EndTime;
	unsigned unTotalTime;
	unsigned unSubTime;		// time cost of sub function(micro sec)
	unsigned unSelfTime;	// time cost without sub function(micro sec)
	unsigned unStartMem;	// memory cost before function start
	unsigned unEndMem;		// memory cost after function start
	unsigned idCaller;		// caller frame id. if no caller, idCaller = id
	P1_StackFrame(){
		id = 0;
		dwAddr = 0;
		i64StartTime = 0;
		i64EndTime = 0;
		unTotalTime = 0;
		unSubTime = 0;
		unSelfTime = 0;
		unStartMem = 0;
		unEndMem = 0;
		idCaller = 0;
	}
};

/**
 * @brief Data of every function execution between FrameStart and FrameEnd
 * 
 */
struct P1_Frame {
	unsigned id;
	__int64 i64StartTime;
	__int64 i64EndTime;
	unsigned unStartMem;
	unsigned unEndMem;
	std::vector<P1_StackFrame> vecStackFrames;
	P1_Frame(){
		id = 0;
		i64StartTime = 0;
		i64EndTime = 0;
		unStartMem = 0;
		unEndMem = 0;
	}
};

/**
 * @brief Statistic of each function execution
 * 
 */
struct P1_StatsUnit {
	DWORD64 dwAddr;
	unsigned unTotalTime;
	unsigned unTotalSlefTime;
	int nTotalMem;
	unsigned unInvokeTimes;
	std::string strName;
	P1_StatsUnit(){
		dwAddr = 0;
		unTotalTime = 0;
		unTotalSlefTime = 0;
		nTotalMem = 0;
		unInvokeTimes = 0;
	}
};

/**
 * @brief Profiler1, a profiler to find out the time & memory cost 
 * of function calls.
 * 
 */
class Profiler1 {

public:
	/**
	 * @brief Get the global Profiler instance
	 * 
	 * @return Profiler1& 
	 */
	static Profiler1& GetInstance();
	static Profiler1* GetInstancePtr();
	~Profiler1();

	const char * echo();

	/**
	 * @brief Start recording
	 * 
	 */
	void Start();

	/**
	 * @brief End recording
	 * 
	 */
	void Stop();

	/**
	 * @brief Mark the start of one frame, pair with FrameEnd
	 * 
	 */
	void FrameStart();

	/**
	 * @brief Mark the end of one frame
	 * 
	 */
	void FrameEnd();


	/**
	 * @brief Analyze the recorded data, should call after Stop()
	 * 
	 */
	void Analyze();

	/**
	 * @brief Get the statistic data of every function, should call after Analyze()
	 * 
	 * @return std::vector<StatsUnit> 
	 */
	std::vector<P1_StatsUnit> GetStatistic();

	/**
	 * @brief Get the statistic data of targe frame, should call after Analyze()
	 * 
	 * @param unFrame targe frame number
	 * @return std::vector<StatsUnit> 
	 */
	std::vector<P1_StatsUnit> GetStatistic(unsigned unFrame);

	/**
	 * @brief Save the statistic data of every function to file, should call after Analyze()
	 * 
	 * @param filename 
	 */
	bool WriteStatistic(const char * filename);

	/**
	 * @brief Save the statistic data of every function of targe frame to file, should call after Analyze()
	 * 
	 * @param filename 
	 */
	bool WriteStatistic(const char * filename, unsigned unFrame);

	/**
	 * @brief Save the statistic data of each frame to file, should call after Analyze()
	 * 
	 * @param filename
	 */
	bool WriteFrameStatistic(const char * filename);

	/**
	 * @brief Get the whole collected data, seperated by frames, should call after Analyze()
	 * 
	 * @return std::vector<Frame> 
	 */
	std::vector<P1_Frame> GetFrames();

	/**
	 * @brief Set the thread id to be record
	 * 
	 */
	void SetTargetThread(DWORD);

	/**
	 * @brief Get name of the function by its address
	 * 
	 * @param dwAddr the address
	 * @return std::string name of the function
	 */
	std::string GetFunctionName(DWORD64 dwAddr);

	/**
	 * @brief Set true to record memory cost, default is false
	 * 
	 */
	bool bEnableMemoryProfile;
	DWORD dwTargetThread;
	__int64 i64StartTime;
	__int64 i64Frequency;
	std::vector<P1_Frame> m_vecFrames;
	std::stack<unsigned> m_stackFrames;
	std::vector<std::string> m_vecMsgs;
	std::map<DWORD64, P1_StatsUnit> m_mapStats;
	std::vector<std::map<DWORD64, P1_StatsUnit>> m_vecStats;
private:
	bool WriteStatistic(std::vector<P1_StatsUnit>& vecStats, const char * filename);
	void StatsCall(unsigned unFrame, P1_StackFrame& frame);

	Profiler1();
	class GC {
	public:
		~GC();
	};
	static GC gc;
	static Profiler1* s_pInstance;

	bool bStart;
	PSYMBOL_INFO pSymbol;
	std::unordered_map<DWORD64, std::string> m_nametable;
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
};

// void _stdcall EnterFunc(unsigned* pStack);

// void _stdcall ExitFunc(unsigned* pStack);


/**
 * @brief Get the global instance of Profiler1
 * 
 */
#define g_objProfiler1 Profiler1::GetInstance()



