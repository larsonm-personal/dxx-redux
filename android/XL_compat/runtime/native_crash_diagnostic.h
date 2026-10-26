// Windows-only diagnostics for the private native qualification harness
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#pragma comment(lib, "dbghelp.lib")
static LONG WINAPI xl_native_crash(EXCEPTION_POINTERS *exception)
{
	const auto process = GetCurrentProcess(), thread = GetCurrentThread();
	SymInitialize(process, nullptr, TRUE);
	CONTEXT context = *exception->ContextRecord;
	STACKFRAME64 frame{};
#ifdef _WIN64
	const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context.Rip;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
#else
	const DWORD machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
	std::fprintf(stderr, "XL native exception %08lx\n", exception->ExceptionRecord->ExceptionCode);
	for (int i = 0; i < 20; ++i) {
		alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 512] = {};
		auto *symbol = reinterpret_cast<SYMBOL_INFO *>(storage);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = 511;
		DWORD64 displacement = 0;
		if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
			std::fprintf(stderr, "  %s + %llu\n", symbol->Name, static_cast<unsigned long long>(displacement));
		else std::fprintf(stderr, "  %llx\n", static_cast<unsigned long long>(frame.AddrPC.Offset));
		if (!StackWalk64(machine, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
	}
	std::fflush(stderr);
	return EXCEPTION_EXECUTE_HANDLER;
}
static void xl_install_crash_diagnostic()
{
	SetUnhandledExceptionFilter(xl_native_crash);
}
#else
static void xl_install_crash_diagnostic() {}
#endif
