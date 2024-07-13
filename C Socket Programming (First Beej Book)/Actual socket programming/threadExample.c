#include <stdio.h>
#include <windows.h>

DWORD WINAPI testing()
{
    printf("aaaaaa");
    return 0;
}

int main(void)
{
    HANDLE thread = CreateThread(NULL, 0, testing, NULL, 0, NULL);
    if (thread)
    {
        WaitForSingleObject(thread, INFINITE);
    }
    //first,  would be some security flags
    //0 = default stack size
    //testing is the function
    //after testing would be parameters of function, NULL if not any params, pointer to the parameters ofc 
    //next is when to run, 0 cuz thread runs immediately.
    //last would be a pointer to a variable that receives THE THREAD ID, useful ig
    //if it succeeds it returns a handle to the created thread

    //DWORD is unsigned long
    //WINAPI is __stdcall
    //__stdcall is the calling convention used for the function.
    // This tells the compiler the rules that apply for setting up the stack, pushing arguments and getting a return value.
    return 0;
}