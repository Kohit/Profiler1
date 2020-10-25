// Profiler1.cpp : msvc implementation of profiler1

/**
* Profiler1 is a c++ profiler, aim to find out the time & memory cost 
* of each function call, with the help of compiler instrumentation,
* using this library doesn't need to modify your source code.
* This profiler could also use to trace the call stack and detect memory leak.
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
* 1. simply compile profiler1 along, to generate (obj/lib/dll) binary.
* 2. compile target source code, with /Gh /GH option, then link the binary.
* for more detail, check https://docs.microsoft.com/en-us/cpp/build
* /reference/gh-enable-penter-hook-function?view=vs-2019
* 
* Example:
* see test.cpp
**/

#include "profiler1.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <Psapi.h>
#include <algorithm>
#include <iomanip>
static bool g_bEnableProfiler1 = false;

Profiler1::GC::~GC()
{
	if (s_pInstance) {
		delete s_pInstance;
		s_pInstance = NULL;
	}
}

Profiler1::Profiler1() {
	// to get the function name, we need to initialize dbghelp.lib
	SymInitialize(GetCurrentProcess(), NULL, TRUE);

	pSymbol = (PSYMBOL_INFO)buffer;

	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	LARGE_INTEGER nFreq;
	QueryPerformanceFrequency(&nFreq);
	i64Frequency = nFreq.QuadPart;

	bStart = false;
	g_bEnableProfiler1 = false;
	bEnableMemoryProfile = false;
	dwTargetThread = GetCurrentThreadId();
}

Profiler1& Profiler1::GetInstance() {
	if (!s_pInstance) {
		s_pInstance = new Profiler1;
	}
	return *s_pInstance;
}

Profiler1* Profiler1::GetInstancePtr()
{
	return s_pInstance;
}

Profiler1::~Profiler1() {
	g_bEnableProfiler1 = false;
	SymCleanup(GetCurrentProcess());
}

std::string Profiler1::GetFunctionName(DWORD64 dwAddr) {
	std::unordered_map<DWORD64, std::string>::iterator it = m_nametable.find(dwAddr);
	if (it != m_nametable.end()) {
		return it->second;
	}
	DWORD64  dwDisplacement = 0;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	if (SymFromAddr(GetCurrentProcess(), dwAddr, &dwDisplacement, pSymbol)) {
		std::string strName = pSymbol->Name;
		m_nametable[dwAddr] = strName;
		return strName;
	}
	else {
		// SymFromAddr failed
		DWORD error = GetLastError();
		std::stringstream ss;
		ss << "#error:Profiler1::GetFunctionName: SymFromAddr:"
		<< dwAddr << " returned error: " << error << std::endl;
		m_vecMsgs.push_back(ss.str());
	}
	return "";
}

const char *  Profiler1::echo() {
	return "echo";
}

void Profiler1::Start()
{
	m_vecFrames.clear();
	m_vecMsgs.clear();
	m_vecStats.clear();
	m_mapStats.clear();
	m_stackFrames = std::stack<unsigned int>();

	bStart = true;

	LARGE_INTEGER StartingTime;
	QueryPerformanceCounter(&StartingTime);

	i64StartTime = StartingTime.QuadPart;
}

void Profiler1::FrameStart()
{
	if (!bStart) {
		return;
	}
	m_vecFrames.push_back(P1_Frame());
	m_stackFrames = std::stack<unsigned int>();

	size_t szFrames = m_vecFrames.size();
	P1_Frame& frame = m_vecFrames[szFrames - 1];
	frame.id = szFrames - 1;
	if (bEnableMemoryProfile){
		PROCESS_MEMORY_COUNTERS infoPMC;

		GetProcessMemoryInfo(GetCurrentProcess(), &infoPMC, sizeof(infoPMC));
		frame.unStartMem = infoPMC.WorkingSetSize;
	}

	LARGE_INTEGER StartingTime;
	QueryPerformanceCounter(&StartingTime);

	frame.i64StartTime = StartingTime.QuadPart;
	g_bEnableProfiler1 = true;
}

void Profiler1::Stop()
{
	g_bEnableProfiler1 = false;
	bStart = false;

	if (!m_stackFrames.empty()) {

		// pop incomplete last frame
		m_vecFrames.pop_back();
	}
}

void Profiler1::FrameEnd()
{
	g_bEnableProfiler1 = false;
	size_t size = m_vecFrames.size();
	if (size <= 0) {
		return;
	}
	P1_Frame& frame = m_vecFrames[size - 1];
	if (bEnableMemoryProfile){
		PROCESS_MEMORY_COUNTERS infoPMC;

		GetProcessMemoryInfo(GetCurrentProcess(), &infoPMC, sizeof(infoPMC));
		frame.unEndMem = infoPMC.WorkingSetSize;
	}

	LARGE_INTEGER EndingTime;
	QueryPerformanceCounter(&EndingTime);

	frame.i64EndTime = EndingTime.QuadPart;
}

void Profiler1::Analyze()
{
	g_bEnableProfiler1 = false;
	m_vecStats.clear();
	m_mapStats.clear();
	
	// for each display frame
	size_t szFrames = m_vecFrames.size();
	for (size_t k = 0; k < szFrames; k++) {
		
		m_vecStats.push_back(std::map<DWORD64, P1_StatsUnit>());

		// rebuild callstack iterativly
		std::stack<unsigned> callStack;
		std::vector<P1_StackFrame>& vecStackFrames = m_vecFrames[k].vecStackFrames;
		size_t size = vecStackFrames.size();

		for (size_t i = 0; i < size; i++) {
			P1_StackFrame & rootFrame = vecStackFrames[i];
			rootFrame.unSubTime = 0;
			callStack.push(rootFrame.id);

			for (size_t j = i + 1; j < size; j++) {
				P1_StackFrame & frameNext = vecStackFrames[j];
				if (frameNext.idCaller == frameNext.id) {
					// idCaller == id means it is rootFrame
					break;
				} else if (frameNext.idCaller != callStack.top()) {

					__int64 i64SubTimeCost = 0;
					while(!callStack.empty()) {
						int idTop = callStack.top();
						if (idTop == frameNext.idCaller) {
							vecStackFrames[idTop].unSubTime += (unsigned)i64SubTimeCost;
							break;
						}
						callStack.pop();

						__int64 i64LocalTimeCost = 0;
						i64LocalTimeCost = vecStackFrames[idTop].i64EndTime - vecStackFrames[idTop].i64StartTime;

						i64LocalTimeCost *= 1000000;
						i64LocalTimeCost /= i64Frequency;
						vecStackFrames[idTop].unTotalTime = (unsigned)i64LocalTimeCost;
						vecStackFrames[idTop].unSubTime += (unsigned)i64SubTimeCost;
						vecStackFrames[idTop].unSelfTime = (unsigned)(i64LocalTimeCost - vecStackFrames[idTop].unSubTime);
						i64SubTimeCost = i64LocalTimeCost;

						StatsCall(k, vecStackFrames[idTop]);
					}
				}
				frameNext.unSubTime = 0;
				callStack.push(frameNext.id);
				i++;
			}

			__int64 i64SubTimeCost = 0;

			while(!callStack.empty()) {
				int idTop = callStack.top();
				callStack.pop();

				__int64 i64LocalTimeCost = 0;
				i64LocalTimeCost = vecStackFrames[idTop].i64EndTime - vecStackFrames[idTop].i64StartTime;

				i64LocalTimeCost *= 1000000;
				i64LocalTimeCost /= i64Frequency;
				vecStackFrames[idTop].unTotalTime = (unsigned)i64LocalTimeCost;
				vecStackFrames[idTop].unSubTime += (unsigned)i64SubTimeCost;
				vecStackFrames[idTop].unSelfTime = (unsigned)(i64LocalTimeCost - vecStackFrames[idTop].unSubTime);
				i64SubTimeCost = i64LocalTimeCost;
				StatsCall(k, vecStackFrames[idTop]);
			}
		}
	}
}


void Profiler1::StatsCall(unsigned unFrame, P1_StackFrame& frame)
{
	std::map<DWORD64, P1_StatsUnit>::iterator it = m_mapStats.find(frame.dwAddr);
	if (it != m_mapStats.end()) {
		it->second.unInvokeTimes++;
		it->second.unTotalSlefTime += frame.unSelfTime;
		it->second.unTotalTime += frame.unTotalTime;
		it->second.nTotalMem += (int)(frame.unEndMem - frame.unStartMem);
	} else {
		P1_StatsUnit unit;
		unit.dwAddr = frame.dwAddr;
		unit.unInvokeTimes = 1;
		unit.unTotalSlefTime = frame.unSelfTime;
		unit.unTotalTime = frame.unTotalTime;
		unit.nTotalMem = (frame.unEndMem - frame.unStartMem);
		unit.strName = GetFunctionName(frame.dwAddr);
		m_mapStats[frame.dwAddr] = unit;
	}

	if (unFrame >= m_vecStats.size()) {
		return;
	}
	std::map<DWORD64, P1_StatsUnit>& stats = m_vecStats[unFrame];

	it = stats.find(frame.dwAddr);
	if (it != stats.end()) {
		it->second.unInvokeTimes++;
		it->second.unTotalSlefTime += frame.unSelfTime;
		it->second.unTotalTime += frame.unTotalTime;
		it->second.nTotalMem += (int)(frame.unEndMem - frame.unStartMem);
	} else {
		P1_StatsUnit unit;
		unit.dwAddr = frame.dwAddr;
		unit.unInvokeTimes = 1;
		unit.unTotalSlefTime = frame.unSelfTime;
		unit.unTotalTime = frame.unTotalTime;
		unit.nTotalMem = (int)(frame.unEndMem - frame.unStartMem);
		unit.strName = GetFunctionName(frame.dwAddr);
		stats[frame.dwAddr] = unit;
	}
}

bool cmp(P1_StatsUnit& sl, P1_StatsUnit& sr) {
	return sl.unTotalSlefTime > sr.unTotalSlefTime;
}

std::vector<P1_StatsUnit> Profiler1::GetStatistic()
{
	std::vector<P1_StatsUnit> vecStats;
	std::map<DWORD64, P1_StatsUnit>::iterator it = m_mapStats.begin();
	for (; it != m_mapStats.end(); it++) {
		vecStats.push_back(it->second);
	}

	std::sort(vecStats.begin(), vecStats.end(), cmp);

	return vecStats;
}

std::vector<P1_StatsUnit> Profiler1::GetStatistic(unsigned unFrame)
{
	if (unFrame >= m_vecStats.size()) {
		return std::vector<P1_StatsUnit>();
	}
	std::map<DWORD64, P1_StatsUnit>& stats = m_vecStats[unFrame];
	std::vector<P1_StatsUnit> vecStats;
	std::map<DWORD64, P1_StatsUnit>::iterator it = stats.begin();
	for (; it != stats.end(); it++) {
		vecStats.push_back(it->second);
	}

	std::sort(vecStats.begin(), vecStats.end(), cmp);

	return vecStats;
}

bool Profiler1::WriteStatistic(const char * filename)
{
	std::vector<P1_StatsUnit> vecStats = GetStatistic();
	return WriteStatistic(vecStats, filename);
}

bool Profiler1::WriteStatistic(const char * filename, unsigned unFrame)
{
	std::vector<P1_StatsUnit> vecStats = GetStatistic(unFrame);
	return WriteStatistic(vecStats, filename);
}

bool Profiler1::WriteStatistic(std::vector<P1_StatsUnit>& vecStats, const char * filename)
{
	std::ofstream ostrm(filename, std::ofstream::trunc);
	ostrm << "\"Address\",\"Name\",\"AvgSelfTime(us)\",\"AvgTime(us)\",\"AvgMemory(bytes)\",\"TotalSelfTime(us)\",\"TotalTime(us)\",\"TotalMemory(bytes)\",\"InvokeTimes\"\n";
    std::ios_base::fmtflags ff, fn;
	ff = ostrm.flags();
	fn = ff;
  	fn &= ~std::cout.basefield;   // unset basefield bits
	fn |= std::ios::hex;
	fn |= std::ios::showbase;
	fn |= std::ios::uppercase;

	for (std::vector<P1_StatsUnit>::iterator it = vecStats.begin(); it != vecStats.end(); it++) {
		ostrm.flags(fn);
		ostrm << "\"" << it->dwAddr;
		ostrm.flags(ff);
		ostrm << "\",\"" << it->strName << "\",\"" 
			<< it->unTotalSlefTime / it->unInvokeTimes << "\",\"" 
			<< it->unTotalTime / it->unInvokeTimes << "\",\"" 
			<< (int)(it->nTotalMem / it->unInvokeTimes) << "\",\""
			<< it->unTotalSlefTime << "\",\"" 
			<< it->unTotalTime << "\",\""
			<< it->nTotalMem << "\",\""
			<< it->unInvokeTimes << "\"\n";
	}
	ostrm.close();
	return true;
}

bool Profiler1::WriteFrameStatistic(const char * filename)
{
	std::ofstream ostrm(filename);
	ostrm << "\"Frame\",\"StartTime\",\"TotalTime(us)\",\"TotalMemory(bytes)\",\"InvokeTimes\"\n";

	for (std::vector<P1_Frame>::iterator it = m_vecFrames.begin(); it != m_vecFrames.end(); it++) {
		__int64 i64LocalTimeCost = 0;
		i64LocalTimeCost = it->i64EndTime - it->i64StartTime;
		i64LocalTimeCost *= 1000000;
		i64LocalTimeCost /= i64Frequency;

		__int64 i64TimeStart = 0;
		i64TimeStart = it->i64StartTime - i64StartTime;
		i64TimeStart *= 1000000;
		i64TimeStart /= i64Frequency;

		ostrm << "\"" << it->id << "\",\"" 
			<< i64TimeStart << "\",\"" 
			<< i64LocalTimeCost << "\",\""
			<< it->unEndMem - it->unStartMem << "\",\""
			<< it->vecStackFrames.size() << "\"\n";
	}
	ostrm.close();
	return true;
}


std::vector<P1_Frame> Profiler1::GetFrames()
{
	return m_vecFrames;
}

void Profiler1::SetTargetThread(DWORD dwThreadId)
{
	dwTargetThread = dwThreadId;
}

Profiler1* Profiler1::s_pInstance = new Profiler1;
Profiler1::GC Profiler1::gc;
Profiler1* s_pProfiler1 = g_objProfiler1.GetInstancePtr();

void _stdcall EnterFunc(unsigned* pStack)
{
	if (GetCurrentThreadId() != s_pProfiler1->dwTargetThread) {
		return;
	}
	size_t szFrame = s_pProfiler1->m_vecFrames.size();
	if (szFrame <= 0) {
		return;
	}

	void* pCaller = (void*)(pStack[0] - 5);

	P1_Frame& frame = s_pProfiler1->m_vecFrames[szFrame - 1];

	P1_StackFrame stackFrame;
	stackFrame.dwAddr = (DWORD64)pCaller;
	stackFrame.id = frame.vecStackFrames.size();

	if (s_pProfiler1->m_stackFrames.empty()) {
		stackFrame.idCaller = stackFrame.id;
	} else {
		stackFrame.idCaller = s_pProfiler1->m_stackFrames.top();
	}

	if (s_pProfiler1->bEnableMemoryProfile){
		PROCESS_MEMORY_COUNTERS infoPMC;

		GetProcessMemoryInfo(GetCurrentProcess(), &infoPMC, sizeof(infoPMC));
		stackFrame.unStartMem = infoPMC.WorkingSetSize;
	}

	LARGE_INTEGER StartingTime;
	QueryPerformanceCounter(&StartingTime);

	stackFrame.i64StartTime = StartingTime.QuadPart;

	frame.vecStackFrames.push_back(stackFrame);
	s_pProfiler1->m_stackFrames.push(stackFrame.id);
}

static __declspec(thread) bool bHooking = false;

extern "C" __declspec(naked) void __cdecl _penter()
{
	// prolog
	_asm
	{
		pushad              // save all general purpose registers
	}
	if (bHooking || !g_bEnableProfiler1) {
		// epilog
		_asm {
			popad
			ret
		}
	}
	bHooking = true;
	_asm {
		mov eax, esp     // current stack pointer
		add eax, 32      // stack pointer before pushad
		push eax		 // return address, the address of the function
		call EnterFunc
	}

	bHooking = false;
	// epilog
	_asm
	{
		popad               // restore general purpose registers
		ret                 // start executing original function
	}
}

void _stdcall ExitFunc(unsigned* pStack)
{
	if (GetCurrentThreadId() != s_pProfiler1->dwTargetThread) {
		return;
	}

	void* pCaller = (void*)(pStack[0] - 5); // the instruction for calling _penter is 5 bytes long

	if (s_pProfiler1->m_stackFrames.empty()){
		return;
	} else {
		size_t szFrame = s_pProfiler1->m_vecFrames.size();
		if (szFrame <= 0) {
			return;
		}
		P1_Frame& frame = s_pProfiler1->m_vecFrames[szFrame - 1];
		unsigned int idFrame = s_pProfiler1->m_stackFrames.top();

		if (s_pProfiler1->bEnableMemoryProfile){
			PROCESS_MEMORY_COUNTERS infoPMC;

			GetProcessMemoryInfo(GetCurrentProcess(), &infoPMC, sizeof(infoPMC));
			frame.vecStackFrames[idFrame].unEndMem = infoPMC.WorkingSetSize;
		}

		LARGE_INTEGER EndTime;
		QueryPerformanceCounter(&EndTime);

		frame.vecStackFrames[idFrame].i64EndTime = EndTime.QuadPart;
		s_pProfiler1->m_stackFrames.pop();
	}

}

extern "C" __declspec(naked) void __cdecl _pexit()
{
	// prolog
	_asm
	{
		pushad              // save all general purpose registers
	}
	if (bHooking || !g_bEnableProfiler1) {
		// epilog
		_asm {
			popad
			ret
		}
	}
	bHooking = true;
	_asm {
		mov    eax, esp     // current stack pointer
		add    eax, 32      // stack pointer before pushad
		push eax			// return address, the last line of the function
		call ExitFunc
	}

	bHooking = false;
	// epilog
	_asm
	{
		popad               // restore general purpose registers
		ret                 // start executing original function
	}
}
