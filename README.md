# Profiler1
Profiler1 is a c++ profiler, aim to find out the time & memory cost 
of each function call, with the help of compiler instrumentation,
using this library doesn't need to modify your source code.
This profiler could also use to trace the call stack and to detect
memory leak.

## Example
```
#include "profiler1.h"

// some code

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
    
    // for better performance, profiler1 won't do analyze during target function call
    // so, we need to run analyze manually, this may takes a while.
    g_objProfiler1.Analyze();

    // write statistic result of each stackframe of frame 2
    g_objProfiler1.WriteStatistic("stats.csv", 2);
    
    // write statistic result of each frame
    g_objProfiler1.WriteFrameStatistic("statsFrame.csv");
    return 0;
}
```
the output may look like:
stats.csv
|Address|Name|AvgSelfTime(us)|AvgTime(us)|AvgMemory(bytes)|TotalSelfTime(us)|TotalTime(us)|TotalMemory(bytes)|InvokeTimes|
|--|--|--|--|--|--|--|--|--|
|0XBDBDB0|allocmemory|205|205|405504|205|205|405504|1|
|0XBD93A0|RunTest|28|263|405504|28|263|405504|1|
|0XBD9460|Foo::Foo|18|30|0|18|30|0|1|
|0XBD9350|Bar::Bar|12|12|0|12|12|0|1|

statsFrame.csv
|Frame|StartTime|TotalTime(us)|TotalMemory(bytes)|InvokeTimes|
|--|--|--|--|--|
|0|43|85|0|4|
|1|153|78|0|4|
|2|260|268|405504|4|
|3|561|1335|4096|107|
|4|1927|1263|0|107|


see test.cpp for more detail.


## Usage:
### Compile with cl:
1. simply compile profiler1.cpp alone, to generate (obj/lib/dll) binary.
2. compile target source code, with ```/Gh /GH``` option, then link the binary.

For more compile detail, check:
https://docs.microsoft.com/en-us/cpp/build/reference/gh-enable-penter-hook-function?view=vs-2019

### Compile with g++:
Not supported yet, you need to add ```-finstrument-functions``` compile option and change the ```_penter/_pexit``` implementation to
```
    void __cyg_profile_func_enter (void *, void *) __attribute__((no_instrument_function));
    void __cyg_profile_func_exit (void *, void *) __attribute__((no_instrument_function));
```


## License
The MIT License

Copyright(C) 2020 kohit (kohits@outlook.com or https://github.com/Kohit)
