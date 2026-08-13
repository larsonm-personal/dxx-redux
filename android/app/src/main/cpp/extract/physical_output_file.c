#include "physical_output_file.h"

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winternl.h>

#ifndef FILE_OPEN_REPARSE_POINT
#define FILE_OPEN_REPARSE_POINT 0x00200000
#endif

typedef NTSTATUS(NTAPI *dxx_nt_create_file_fn)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
    PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);

static int handle_is_plain(HANDLE handle, int require_directory)
{
	FILE_ATTRIBUTE_TAG_INFO attributes;
	FILE_STANDARD_INFO standard;
	BY_HANDLE_FILE_INFORMATION identity;

	if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo,
	                                  &attributes, sizeof(attributes)) ||
	    (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
	    !GetFileInformationByHandleEx(handle, FileStandardInfo,
	                                  &standard, sizeof(standard)) ||
	    standard.Directory != require_directory ||
	    (!require_directory &&
	     (!GetFileInformationByHandle(handle, &identity) ||
	      identity.nNumberOfLinks != 1)))
		return 0;
	return 1;
}

static HANDLE open_relative(HANDLE parent, const char *name,
                            ACCESS_MASK access, ULONG disposition,
                            ULONG options)
{
	static dxx_nt_create_file_fn nt_create_file;
	wchar_t wide_name[256];
	UNICODE_STRING unicode_name;
	OBJECT_ATTRIBUTES object_attributes;
	IO_STATUS_BLOCK io_status;
	HANDLE handle = INVALID_HANDLE_VALUE;
	int wide_length;
	NTSTATUS status;

	if (!nt_create_file) {
		HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		if (!ntdll)
			return INVALID_HANDLE_VALUE;
		nt_create_file = (dxx_nt_create_file_fn) (void *)
		    GetProcAddress(ntdll, "NtCreateFile");
		if (!nt_create_file)
			return INVALID_HANDLE_VALUE;
	}
	wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1,
	                                  wide_name, 256);
	if (wide_length <= 1)
		return INVALID_HANDLE_VALUE;
	unicode_name.Buffer = wide_name;
	unicode_name.Length = (USHORT) ((wide_length - 1) * sizeof(wchar_t));
	unicode_name.MaximumLength = (USHORT) (wide_length * sizeof(wchar_t));
	InitializeObjectAttributes(&object_attributes, &unicode_name,
	                           OBJ_CASE_INSENSITIVE, parent, NULL);
	status = nt_create_file(
	    &handle, access, &object_attributes, &io_status, NULL,
	    FILE_ATTRIBUTE_NORMAL,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    disposition, options | FILE_OPEN_REPARSE_POINT | FILE_SYNCHRONOUS_IO_NONALERT,
	    NULL, 0);
	return status >= 0 ? handle : INVALID_HANDLE_VALUE;
}

static HANDLE open_root(const char *output_dir)
{
	HANDLE root = CreateFileA(
	    output_dir,
	    FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
	        FILE_READ_ATTRIBUTES | SYNCHRONIZE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
	    OPEN_EXISTING,
	    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if (root != INVALID_HANDLE_VALUE && !handle_is_plain(root, 1)) {
		CloseHandle(root);
		root = INVALID_HANDLE_VALUE;
	}
	return root;
}

int dxx_physical_output_open(dxx_physical_output_file_t *file,
                             const char *output_dir,
                             const char *relative_path)
{
	char path[512];
	char *component;
	HANDLE parent;

	if (!file || !output_dir || !*output_dir || !relative_path ||
	    !*relative_path || strlen(relative_path) >= sizeof(path))
		return -1;
	memset(file, 0, sizeof(*file));
	file->fd = file->parent_fd = -1;
	memcpy(path, relative_path, strlen(relative_path) + 1);
	parent = open_root(output_dir);
	if (parent == INVALID_HANDLE_VALUE)
		return -1;
	component = path;
	for (;;) {
		char *separator = strchr(component, '/');
		HANDLE next;
		if (!*component) {
			CloseHandle(parent);
			return -1;
		}
		if (!separator)
			break;
		*separator = '\0';
		next = open_relative(
		    parent, component,
		    FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
		        FILE_READ_ATTRIBUTES | SYNCHRONIZE,
		    FILE_OPEN_IF, FILE_DIRECTORY_FILE);
		CloseHandle(parent);
		if (next == INVALID_HANDLE_VALUE || !handle_is_plain(next, 1)) {
			if (next != INVALID_HANDLE_VALUE)
				CloseHandle(next);
			return -1;
		}
		parent = next;
		component = separator + 1;
	}

	file->handle = open_relative(
	    parent, component,
	    GENERIC_WRITE | DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
	    FILE_OPEN, FILE_NON_DIRECTORY_FILE);
	if (file->handle == INVALID_HANDLE_VALUE) {
		file->handle = open_relative(
		    parent, component,
		    GENERIC_WRITE | DELETE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
		    FILE_CREATE, FILE_NON_DIRECTORY_FILE);
	}
	CloseHandle(parent);
	if (file->handle == INVALID_HANDLE_VALUE ||
	    !handle_is_plain((HANDLE) file->handle, 0)) {
		if (file->handle != INVALID_HANDLE_VALUE)
			CloseHandle((HANDLE) file->handle);
		file->handle = NULL;
		return -1;
	}
	{
		LARGE_INTEGER zero;
		zero.QuadPart = 0;
		if (!SetFilePointerEx((HANDLE) file->handle, zero, NULL, FILE_BEGIN) ||
		    !SetEndOfFile((HANDLE) file->handle)) {
			dxx_physical_output_abort(file);
			return -1;
		}
	}
	return 0;
}

int dxx_physical_output_write(dxx_physical_output_file_t *file,
                              const void *data, size_t size)
{
	DWORD written;
	if (!file || !file->handle || size > MAXDWORD ||
	    !WriteFile((HANDLE) file->handle, data, (DWORD) size, &written, NULL) ||
	    written != (DWORD) size)
		return -1;
	return 0;
}

int dxx_physical_output_finish(dxx_physical_output_file_t *file)
{
	int result;
	if (!file || !file->handle)
		return -1;
	result = CloseHandle((HANDLE) file->handle) ? 0 : -1;
	file->handle = NULL;
	return result;
}

void dxx_physical_output_abort(dxx_physical_output_file_t *file)
{
	if (file && file->handle) {
		FILE_DISPOSITION_INFO disposition;
		disposition.DeleteFile = TRUE;
		SetFileInformationByHandle((HANDLE) file->handle, FileDispositionInfo,
		                           &disposition, sizeof(disposition));
		CloseHandle((HANDLE) file->handle);
		file->handle = NULL;
	}
}

#else

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static int open_directory_at(int parent_fd, const char *name)
{
	int fd;
	struct stat status;
	if (mkdirat(parent_fd, name, 0755) != 0 && errno != EEXIST)
		return -1;
	fd = openat(parent_fd, name,
	            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0 || fstat(fd, &status) != 0 || !S_ISDIR(status.st_mode)) {
		if (fd >= 0)
			close(fd);
		return -1;
	}
	return fd;
}

int dxx_physical_output_open(dxx_physical_output_file_t *file,
                             const char *output_dir,
                             const char *relative_path)
{
	char path[512];
	char *component;
	int parent;
	struct stat status;
	int flags;

	if (!file || !output_dir || !*output_dir || !relative_path ||
	    !*relative_path || strlen(relative_path) >= sizeof(path))
		return -1;
	memset(file, 0, sizeof(*file));
	file->fd = file->parent_fd = -1;
	memcpy(path, relative_path, strlen(relative_path) + 1);
	parent = open(output_dir,
	              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (parent < 0 || fstat(parent, &status) != 0 ||
	    !S_ISDIR(status.st_mode)) {
		if (parent >= 0)
			close(parent);
		return -1;
	}
	component = path;
	for (;;) {
		char *separator = strchr(component, '/');
		int next;
		if (!*component) {
			close(parent);
			return -1;
		}
		if (!separator)
			break;
		*separator = '\0';
		next = open_directory_at(parent, component);
		close(parent);
		if (next < 0)
			return -1;
		parent = next;
		component = separator + 1;
	}
	if (strlen(component) >= sizeof(file->leaf)) {
		close(parent);
		return -1;
	}
	memcpy(file->leaf, component, strlen(component) + 1);
	file->fd = openat(parent, component,
	                  O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK |
	                      O_CLOEXEC,
	                  0644);
	if (file->fd < 0 || fstat(file->fd, &status) != 0 ||
	    !S_ISREG(status.st_mode) || status.st_nlink != 1 ||
	    ftruncate(file->fd, 0) != 0) {
		if (file->fd >= 0)
			close(file->fd);
		close(parent);
		file->fd = -1;
		return -1;
	}
	flags = fcntl(file->fd, F_GETFL);
	if (flags < 0 || fcntl(file->fd, F_SETFL, flags & ~O_NONBLOCK) != 0) {
		close(file->fd);
		close(parent);
		file->fd = -1;
		return -1;
	}
	file->parent_fd = parent;
	return 0;
}

int dxx_physical_output_write(dxx_physical_output_file_t *file,
                              const void *data, size_t size)
{
	const unsigned char *bytes = (const unsigned char *) data;
	size_t done = 0;
	if (!file || file->fd < 0)
		return -1;
	while (done < size) {
		ssize_t result = write(file->fd, bytes + done, size - done);
		if (result < 0 && errno == EINTR)
			continue;
		if (result <= 0)
			return -1;
		done += (size_t) result;
	}
	return 0;
}

int dxx_physical_output_finish(dxx_physical_output_file_t *file)
{
	int result;
	if (!file || file->fd < 0 || file->parent_fd < 0)
		return -1;
	result = close(file->fd);
	file->fd = -1;
	if (close(file->parent_fd) != 0)
		result = -1;
	file->parent_fd = -1;
	return result;
}

void dxx_physical_output_abort(dxx_physical_output_file_t *file)
{
	if (!file)
		return;
	if (file->parent_fd >= 0 && file->leaf[0])
		unlinkat(file->parent_fd, file->leaf, 0);
	if (file->fd >= 0)
		close(file->fd);
	if (file->parent_fd >= 0)
		close(file->parent_fd);
	file->fd = file->parent_fd = -1;
}

#endif
