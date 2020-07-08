#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

/* Bring in definitions for `PATH_MAX` and `PATHSEPSTRING`, `jl_ptls_t`, etc... */
#include "../src/julia.h"

#ifdef _OS_WINDOWS_
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

/* Define ptls getter, as this cannot be defined within a shared library. */
#if !defined(_OS_WINDOWS_) && !defined(_OS_DARWIN_)
JL_DLLEXPORT JL_CONST_FUNC jl_ptls_t jl_get_ptls_states_static(void)
{
    static __attribute__((tls_model("local-exec"))) __thread jl_tls_states_t tls_states;
    return &tls_states;
}
#endif

/*
 * DEP_LIBS is our list of dependent libraries that must be loaded before `libjulia`.
 * Note that order matters, as each entry will be opened in-order.  We define here a
 * dummy value just so this file compiles on its own, and also so that developers can
 * see what this value should look like.  Note that the last entry must always be
 * `libjuliarepl`, and that all paths should be relative to this loader `.exe` path.
 */
#if !defined(DEP_LIBS)
#define DEP_LIBS "../lib/example.so", "../lib/libjuliarepl.so"
#endif

static const char * dep_libs[] = {
    DEP_LIBS
};

/* Utilities to convert from Windows' wchar_t stuff to UTF-8 */
#ifdef _OS_WINDOWS_
static int wchar_to_utf8(wchar_t * wstr, char **str) {
    size_t len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (!len)
        return 1;

    *str = (char *)alloca(len);
    if (!WideCharToMultiByte(CP_UTF8, 0, wstr, -1, *str, len, NULL, NULL))
        return 1;
    return 0;
}

static int utf8_to_wchar(char * str, wchar_t ** wstr) {
    size_t len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (!len)
        return 1;
    *wstr = (wchar_t *)alloca(len * sizeof(wchar_t));
    if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len))
        return 1;
    return 0;
}
#endif

/* Absolute path to the path of the current executable, gets filled in by `get_exe_path()` */
char * exe_dir = NULL;
static void * load_library(const char * rel_path) {
    char path[2*PATH_MAX + 1];
    snprintf(path, sizeof(path)/sizeof(char), "%s%s%s", exe_dir, PATHSEPSTRING, rel_path);

    void * handle = NULL;
#if defined(_OS_WINDOWS_)
    wchar_t * wpath = NULL;
    if (!utf8_to_wchar(path, &wpath)) {
        fprintf(stderr, "ERROR: Unable to convert path %s to wide string!\n", path);
        exit(1);
    }
    handle = (void *)LoadLibraryExW(wpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#endif

    if (handle == NULL) {
        fprintf(stderr, "ERROR: Unable to load dependent library %s\n", path);
        exit(1);
    }
    return handle;
}

static void get_exe_path() {
#if defined(_OS_WINDOWS_)
    // On Windows, we use GetModuleFileName()
    wchar_t julia_path[PATH_MAX];
    if (!GetModuleFileName(NULL, julia_path, PATH_MAX)) {
        fprintf(stderr, "ERROR: GetModuleFileName() failed with code %lu\n", GetLastError());
        exit(1);
    }
    if (!wchar_to_utf8(julia_path, &exe_dir)) {
        fprintf(stderr, "ERROR: Unable to convert julia path to UTF-8\n");
        exit(1);
    }
#elif defined(_OS_DARWIN_)
    // On MacOS, we use _NSGetExecutablePath(), followed by realpath()
    char nonreal_exe_path[PATH_MAX + 1];
    uint32_t exe_path_len = PATH_MAX;
    int ret = _NSGetExecutablePath(nonreal_exe_path, &exe_path_len);
    if (!ret) {
        fprintf(stderr, "ERROR: _NSGetExecutablePath() returned %d\n", ret);
        exit(1);
    }

    /* realpath(nonreal_exe_path) may be > PATH_MAX so double it to be on the safe side. */
    exe_dir = (char *)malloc(2*PATH_MAX + 1);
    if (realpath(nonreal_exe_path, exe_dir) == NULL) {
        fprintf(stderr, "ERROR: realpath() failed with code %d\n", errno);
        exit(1);
    }
#elif defined(_OS_LINUX_)
    // On Linux, we read from /proc/self/exe
    exe_dir = (char *)malloc(2*PATH_MAX + 1);
    int num_bytes = readlink("/proc/self/exe", exe_dir, PATH_MAX);
    if (num_bytes == -1) {
        fprintf(stderr, "ERROR: readlink(/proc/self/exe) failed with code %d\n", errno);
        exit(1);
    }
    exe_dir[num_bytes] = '\0';
#elif defined(_OS_FREEBSD_)
    // On FreeBSD, we use the KERN_PROC_PATHNAME sysctl:
    exe_dir = (char *) malloc(2*PATH_MAX + 1);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    int exe_dir_len = 2*PATH_MAX;
    int ret = sysctl(mib, 4, exe_dir, &exe_dir_len, NULL, 0);
    if (ret) {
        fprintf(stderr, "ERROR: sysctl(KERN_PROC_PATHNAME) failed with code %d\n", ret);
        exit(1);
    }
    exe_dir[exe_dir_len] = '\0';
#endif

    // Finally, convert to dirname
    exe_dir = dirname(exe_dir);
}

#ifdef _OS_WINDOWS_
int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
#else
int main(int argc, char * argv[])
#endif
{
    // Immediately get the current exe path, allowing us to calculate relative paths.
    get_exe_path();

#ifdef _OS_WINDOWS_
    // Convert Windows wchar_t values to UTF8
    for (int i=0; i<argc; i++) {
        char * new_argv_i = NULL;
        if (!wchar_to_utf8(argv[i], &new_argv_i)) {
            fprintf(STDERR, "Unable to convert %d'th argument to UTF-8!\n", i);
            return 1;
        }
        argv[i] = (wchar_t *)new_argv_i;
    }
#endif

    // Pre-load libraries that libjulia needs.
    int dep_idx;
    for (dep_idx=0; dep_idx<sizeof(dep_libs)/sizeof(dep_libs[0]) - 1; ++dep_idx) {
        load_library(dep_libs[dep_idx]);
    }

    // Finally, load libjuliarepl, then jump into its main() method:
    void * libjuliarepl = load_library(dep_libs[dep_idx]);
    int (*main_fptr)(int, char **) = NULL;
    #ifdef _OS_WINDOWS_
        main_fptr = (int (*)(int, char **))GetProcAddress((HMODULE) libjuliarepl, "fake_main");
    #else
        main_fptr = (int (*)(int, char **))dlsym(libjuliarepl, "fake_main");
    #endif
    if (main_fptr == NULL) {
        fprintf(stderr, "ERROR: Unable to find `main()` within libjuliarepl!\n");
        exit(1);
    }
    return main_fptr(argc, (char **)argv);
}
