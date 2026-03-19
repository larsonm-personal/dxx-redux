/*
 * android_crash_handler.c -- native signal handler for crash reporting
 *
 * Installs signal handlers for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL.
 * On crash, writes a minimal report using only async-signal-safe functions
 * (open, write, close, _exit) to a pre-computed path in the crashlogs dir.
 *
 * The crash file is then available to the Kotlin CrashLog system for
 * display and export in the Advanced Settings page.
 */

#ifdef ANDROID

#include "android_crash_handler.h"
#include <jni.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <android/log.h>

#define TAG "CrashHandler"

/* Pre-computed path buffer for the crash file.
 * Set during init so the signal handler doesn't need to allocate. */
static char s_crash_dir[512];
static int s_initialized = 0;

/* Previous signal handlers to chain to */
static struct sigaction s_old_sigsegv;
static struct sigaction s_old_sigabrt;
static struct sigaction s_old_sigbus;
static struct sigaction s_old_sigfpe;
static struct sigaction s_old_sigill;

/* Async-signal-safe integer-to-string (decimal). Returns pointer into buf. */
static char *itoa_safe(long val, char *buf, int buflen)
{
	char *p = buf + buflen - 1;
	int neg = 0;
	unsigned long uval;

	*p = '\0';
	if (val < 0) {
		neg = 1;
		uval = (unsigned long) (-(val + 1)) + 1;
	} else {
		uval = (unsigned long) val;
	}
	if (uval == 0) {
		*(--p) = '0';
	} else {
		while (uval > 0 && p > buf) {
			*(--p) = '0' + (char) (uval % 10);
			uval /= 10;
		}
	}
	if (neg && p > buf)
		*(--p) = '-';
	return p;
}

/* Async-signal-safe hex formatter. Returns pointer into buf. */
static char *hex_safe(unsigned long val, char *buf, int buflen)
{
	static const char hex[] = "0123456789abcdef";
	char *p = buf + buflen - 1;
	*p = '\0';
	if (val == 0) {
		*(--p) = '0';
	} else {
		while (val > 0 && p > buf) {
			*(--p) = hex[val & 0xf];
			val >>= 4;
		}
	}
	if (p > buf + 1) {
		*(--p) = 'x';
		*(--p) = '0';
	}
	return p;
}

static const char *signal_name(int sig)
{
	switch (sig) {
		case SIGSEGV: return "SIGSEGV";
		case SIGABRT: return "SIGABRT";
		case SIGBUS: return "SIGBUS";
		case SIGFPE: return "SIGFPE";
		case SIGILL: return "SIGILL";
		default: return "UNKNOWN";
	}
}

static const char *sigcode_name(int sig, int code)
{
	if (sig == SIGSEGV) {
		switch (code) {
			case SEGV_MAPERR: return "SEGV_MAPERR (address not mapped)";
			case SEGV_ACCERR: return "SEGV_ACCERR (invalid permissions)";
		}
	} else if (sig == SIGBUS) {
		switch (code) {
			case BUS_ADRALN: return "BUS_ADRALN (alignment error)";
			case BUS_ADRERR: return "BUS_ADRERR (nonexistent address)";
			case BUS_OBJERR: return "BUS_OBJERR (object-specific error)";
		}
	} else if (sig == SIGFPE) {
		switch (code) {
			case FPE_INTDIV: return "FPE_INTDIV (integer divide by zero)";
			case FPE_FLTDIV: return "FPE_FLTDIV (float divide by zero)";
			case FPE_FLTOVF: return "FPE_FLTOVF (float overflow)";
		}
	}
	return "unknown";
}

static void crash_handler(int sig, siginfo_t *info, void *ucontext)
{
	if (!s_initialized) goto chain;

	{
		/* Build crash file path: <crashdir>/crash_native_<pid>.txt
		 * Using pid makes it unique enough for signal context. */
		char path[600];
		char numbuf[24];
		int fd;
		const char *num;

		memset(path, 0, sizeof(path));
		strncat(path, s_crash_dir, sizeof(path) - 40);
		strcat(path, "/crash_native_");
		num = itoa_safe((long) getpid(), numbuf, sizeof(numbuf));
		strcat(path, num);
		strcat(path, ".txt");

		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd >= 0) {
			const char *name = signal_name(sig);
			const char *cname;
			char *hex;
			char hexbuf[24];

			write(fd, "Native crash report\n", 20);
			write(fd, "Signal: ", 8);
			write(fd, name, strlen(name));

			write(fd, " (", 2);
			num = itoa_safe((long) sig, numbuf, sizeof(numbuf));
			write(fd, num, strlen(num));
			write(fd, ")\n", 2);

			write(fd, "Code: ", 6);
			cname = sigcode_name(sig, info ? info->si_code : 0);
			write(fd, cname, strlen(cname));
			write(fd, "\n", 1);

			if (info && (sig == SIGSEGV || sig == SIGBUS)) {
				write(fd, "Fault address: ", 15);
				hex = hex_safe((unsigned long) info->si_addr, hexbuf, sizeof(hexbuf));
				write(fd, hex, strlen(hex));
				write(fd, "\n", 1);
			}

			write(fd, "PID: ", 5);
			num = itoa_safe((long) getpid(), numbuf, sizeof(numbuf));
			write(fd, num, strlen(num));
			write(fd, "\nTID: ", 6);
			num = itoa_safe((long) gettid(), numbuf, sizeof(numbuf));
			write(fd, num, strlen(num));
			write(fd, "\n", 1);

			close(fd);
		}
	}

chain:
	/* Restore and re-raise so the default handler runs (produces tombstone) */
	{
		struct sigaction *old = NULL;
		switch (sig) {
			case SIGSEGV: old = &s_old_sigsegv; break;
			case SIGABRT: old = &s_old_sigabrt; break;
			case SIGBUS: old = &s_old_sigbus; break;
			case SIGFPE: old = &s_old_sigfpe; break;
			case SIGILL: old = &s_old_sigill; break;
		}
		if (old) {
			sigaction(sig, old, NULL);
		} else {
			signal(sig, SIG_DFL);
		}
		raise(sig);
	}
}

void android_crash_handler_init(const char *crash_dir)
{
	struct sigaction sa;

	if (!crash_dir || strlen(crash_dir) >= sizeof(s_crash_dir) - 1) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
		                    "crash_dir path too long or null");
		return;
	}
	strncpy(s_crash_dir, crash_dir, sizeof(s_crash_dir) - 1);
	s_crash_dir[sizeof(s_crash_dir) - 1] = '\0';
	s_initialized = 1;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = crash_handler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGSEGV, &sa, &s_old_sigsegv);
	sigaction(SIGABRT, &sa, &s_old_sigabrt);
	sigaction(SIGBUS, &sa, &s_old_sigbus);
	sigaction(SIGFPE, &sa, &s_old_sigfpe);
	sigaction(SIGILL, &sa, &s_old_sigill);

	__android_log_print(ANDROID_LOG_INFO, TAG,
	                    "Native crash handler installed (dir=%s)", crash_dir);
}

/* JNI entry point called from CrashLog.kt */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_CrashLog_nativeInstallCrashHandler(JNIEnv *env, jobject thiz,
                                                         jstring crash_dir)
{
	const char *dir = (*env)->GetStringUTFChars(env, crash_dir, NULL);
	if (dir) {
		android_crash_handler_init(dir);
		(*env)->ReleaseStringUTFChars(env, crash_dir, dir);
	}
}

#endif /* ANDROID */
