#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>

__declspec(dllexport) __attribute__((naked)) void* BmlFakeAddItemToVoidChestServer(int player, void* item, bool forceNewStack, void* parent)
{
    (void)player;
    (void)item;
    (void)forceNewStack;
    (void)parent;
    __asm__(
        ".byte 0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90\n"
        "mov %rdx, %rax\n"
        "mov %ecx, %r10d\n"
        "add %r10, %rax\n"
        "movzbl %r8b, %r11d\n"
        "add %r11, %rax\n"
        "add %r9, %rax\n"
        "add $17, %rax\n"
        "ret\n"
    );
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}
