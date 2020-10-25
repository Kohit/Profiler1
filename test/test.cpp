// test.cpp: usage example of profiler1

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
**/

#include <iostream>
#include "..\Profiler1\profiler1.h"

const char * echo() {
    return "echo";
}

int a = 0;
void bla() {
    a++;
}

class Bar {
public:
    Bar() {

    }
    int add(int a) {
        a++;
        bla();
        bla();
        for (int i = 1; i < 100; i++) {
            echo();
        }
        return a;
    }
};
class Foo {
public:
    Foo() {
        b = 0;
    }
    int add(int a) {
        b += bar.add(a);
        bla();
        return b;
    }
private:
    int b;
    Bar bar;
};

// a memory leak function
void allocmemory(){
    int * arr = new int[100000];
}
void RunTest(int i){
    Foo foo;
    if (i < 2) {
        bla();
    } else if (i == 2){
        allocmemory();
    } else {
        int b = foo.add(a);
    }
}

int main()
{
    // test lib load surcessful
    std::cout << g_objProfiler1.echo() << std::endl;
    std::cout << g_objProfiler1.GetFunctionName((DWORD64)main) << std::endl;

    g_objProfiler1.bEnableMemoryProfile = true;
    g_objProfiler1.Start();

    // suppose we have 5 frame
    for (int i = 0; i < 5; i++) {
        // start profiler logging
        g_objProfiler1.FrameStart();

        // do something in this frame
        RunTest(i);

        // end profiler logging
        g_objProfiler1.FrameEnd();
    }

    g_objProfiler1.Stop();
    
    // for better performance, profiler won't do analyze during target function call
    // so, we need to run analyze manually, this may takes a while.
    g_objProfiler1.Analyze();

    // write statistic result of each stackframe of frame 2
    g_objProfiler1.WriteStatistic("stats.csv", 2);
    // result may look like:
    /*
    "Address","Name","AvgTime(us)","AvgMemory(bytes)","TotalTime(us)","TotalMemory(bytes)","InvokeTimes"
    "0X104D30","RunTest","134","405504","134","405504","1"
    "0X1063E0","allocmemory","113","405504","113","405504","1"
    "0X1034A0","Foo::Foo","10","0","10","0","1"
    "0X103430","Bar::Bar","4","0","4","0","1"
    */
    // we've just found a memory leak!

    // write statistic result of each frame
    g_objProfiler1.WriteFrameStatistic("statsFrame.csv");
    /* result may look like:
    "id","StartTime","TotalTime(us)","TotalMemory(bytes)","InvokeTimes"
    "0","18","37","4096","4"
    "1","67","26","0","4"
    "2","106","136","405504","4"
    "3","258","490","0","107"
    "4","766","459","0","107"
    */
    // memory increased 405504 bytes after frame 2

    // below shows how to trace caller for every function call in frame 0
    std::vector<P1_StackFrame> stackFrames = g_objProfiler1.GetFrames()[0].vecStackFrames;
    {
        for (size_t i = 0; i < stackFrames.size(); i++) {
            P1_StackFrame & frame = stackFrames[i];
            std::cout << ">" << g_objProfiler1.GetFunctionName(frame.dwAddr) << "[" << std::hex << frame.dwAddr << "]" << std::endl;
            unsigned idCaller = frame.idCaller;
            unsigned idFrame = frame.id;
            bool bOut = false;
            // check if its a root frame
            if (idCaller != idFrame) {
                bOut = true;
                std::cout << "--------------------------" << std::endl;
                std::cout << "caller stack:" << std::endl;
            }
            
            // trace back
            while (idCaller != idFrame) {
                P1_StackFrame& caller = stackFrames[idCaller];
                std::cout << g_objProfiler1.GetFunctionName(caller.dwAddr) << "[" << std::hex << caller.dwAddr << "]" << std::endl;
                idFrame = caller.id;
                idCaller = caller.idCaller;
            }
            if (bOut) {
                std::cout << "--------------------------" << std::endl;
            }
            std::cout << std::endl;

        }
    }
    /*
    >RunTest[a94d30]

    >Foo::Foo[a934a0]
    --------------------------
    caller stack:
    RunTest[a94d30]
    --------------------------

    >Bar::Bar[a93430]
    --------------------------
    caller stack:
    Foo::Foo[a934a0]
    RunTest[a94d30]
    --------------------------

    >bla[a96480]
    --------------------------
    caller stack:
    RunTest[a94d30]
    --------------------------
    */

    return 0;
}

