SRCDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
JULIAHOME := $(abspath $(SRCDIR)/..)
BUILDDIR ?= .
include $(JULIAHOME)/deps/Versions.make
include $(JULIAHOME)/Make.inc
include $(JULIAHOME)/deps/llvm-ver.make

override CFLAGS += $(JCFLAGS)
override CXXFLAGS += $(JCXXFLAGS)
override CPPFLAGS += $(JCPPFLAGS)

SRCS := cli

HEADERS := $(addprefix $(JULIAHOME)/src/,julia.h julia_assert.h julia_threads.h julia_internal.h options.h) \
	$(BUILDDIR)/../src/julia_version.h $(wildcard $(JULIAHOME)/src/support/*.h) $(LIBUV_INC)/uv.h

FLAGS := -I$(BUILDROOT)/src -I$(JULIAHOME)/src -I$(JULIAHOME)/src/support -I$(build_includedir)
ifneq ($(USEMSVC), 1)
FLAGS += -Wall -Wno-strict-aliasing -fno-omit-frame-pointer -Wc++-compat
endif

OBJS := $(SRCS:%=$(BUILDDIR)/%.o)
DOBJS := $(SRCS:%=$(BUILDDIR)/%.dbg.obj)
DEBUGFLAGS += $(FLAGS)
SHIPFLAGS += $(FLAGS)
JLDFLAGS += $(LDFLAGS) $(NO_WHOLE_ARCHIVE) $(OSLIBS) $(RPATH)

ifeq ($(USE_SYSTEM_LIBM),0)
ifneq ($(UNTRUSTED_SYSTEM_LIBM),0)
JLDFLAGS += $(WHOLE_ARCHIVE) $(build_libdir)/libopenlibm.a $(NO_WHOLE_ARCHIVE)
endif
endif

ifeq ($(OS),WINNT)
JLDFLAGS += -municode
endif

default: release
all: release debug
release debug :  % : julia-%

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@$(call PRINT_CC, $(CC) $(CPPFLAGS) $(CFLAGS) $(SHIPFLAGS) -c $< -o $@)
$(BUILDDIR)/%.dbg.obj: $(SRCDIR)/%.c $(HEADERS)
	@$(call PRINT_CC, $(CC) $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) -c $< -o $@)

ifeq ($(OS),WINNT)
ifneq ($(USEMSVC), 1)
$(BUILDDIR)/julia_res.o: $(JULIAHOME)/contrib/windows/julia.rc $(JULIAHOME)/VERSION
	JLVER=`cat $(JULIAHOME)/VERSION` && \
	JLVERi=`echo $$JLVER | perl -nle \
		'/^(\d+)\.?(\d*)\.?(\d*)/ && \
		print int $$1,",",int $$2,",0,",int $$3'` && \
	$(CROSS_COMPILE)windres $< -O coff -o $@ -DJLVER=$$JLVERi -DJLVER_STR=\\\"$$JLVER\\\"
OBJS += julia_res.o
DOBJS += julia_res.o
endif
endif

julia-release: $(build_bindir)/julia$(EXE)
julia-debug: $(build_bindir)/julia-debug$(EXE)

# Embed an Info.plist in the julia executable
# Create an intermediate target Info.plist for Darwin code signing.
ifeq ($(DARWIN_FRAMEWORK),1)
$(BUILDDIR)/Info.plist: $(JULIAHOME)/VERSION
	/usr/libexec/PlistBuddy -x -c "Clear dict" $@
	/usr/libexec/PlistBuddy -x -c "Add :CFBundleName string julia" $@
	/usr/libexec/PlistBuddy -x -c "Add :CFBundleIdentifier string $(darwin_codesign_id_julia_ui)" $@
	/usr/libexec/PlistBuddy -x -c "Add :CFBundleInfoDictionaryVersion string 6.0" $@
	/usr/libexec/PlistBuddy -x -c "Add :CFBundleVersion string $(JULIA_COMMIT)" $@
	/usr/libexec/PlistBuddy -x -c "Add :CFBundleShortVersionString string $(JULIA_MAJOR_VERSION).$(JULIA_MINOR_VERSION).$(JULIA_PATCH_VERSION)" $@
.INTERMEDIATE: $(BUILDDIR)/Info.plist # cleanup this file after we are done using it
JLDFLAGS += -Wl,-sectcreate,__TEXT,__info_plist,Info.plist
$(build_bindir)/julia$(EXE): $(BUILDDIR)/Info.plist
$(build_bindir)/julia-debug$(EXE): $(BUILDDIR)/Info.plist
endif

ifneq ($(USEMSVC), 1)
CXXLD := $(CXX)
else
CXXLD := $(LD)
endif

$(build_shlibdir)/libjuliacli.$(JL_MAJOR_MINOR_SHLIB_EXT): $(OBJS)
	@$(call PRINT_LINK, $(CXXLD) -shared $(CXXFLAGS) $(CXXLDFLAGS) $(LINK_FLAGS) $(SHIPFLAGS) $(OBJS) -o $@ -L$(build_private_libdir) -L$(build_libdir) -L$(build_shlibdir) -ljulia $(JLDFLAGS) $(CXXLDFLAGS))

$(build_shlibdir)/libjuliacli-debug.$(JL_MAJOR_MINOR_SHLIB_EXT): $(DOBJS)
	@$(call PRINT_LINK, $(CXXLD) -shared $(CXXFLAGS) $(CXXLDFLAGS) $(LINK_FLAGS) $(DEBUGFLAGS) $(DOBJS) -o $@ -L$(build_private_libdir) -L$(build_libdir) -L$(build_shlibdir) -ljulia-debug $(JLDFLAGS) $(CXXLDFLAGS))


ifeq ($(OS),WINNT)
define dep_libs_gen
TEXT(\"$$(echo $(call rel_path,$(build_bindir),$(1)) | sed -e 's_/_\\\\_g')\")
endef
else
define dep_libs_gen
\"$$(echo $(call rel_path,$(build_bindir),$(1)))\"
endef
endif

LOADER_CFLAGS :=
LOADER_LDFLAGS :=

# Eventually, these will live in separate artifacts
LIBGCC_DEPLIB := $(word 1,$(wildcard $(build_shlibdir)/libgcc*.$(SHLIB_EXT)*))
LIBLLVM_DEPLIB := $(word 1,$(wildcard $(build_shlibdir)/libLLVM*jl.$(SHLIB_EXT)*))
LIBJULIACLI_DEPLIB := $(build_shlibdir)/libjuliacli.$(JL_MAJOR_MINOR_SHLIB_EXT)
LIBJULIACLI_DEBUG_DEPLIB := $(build_shlibdir)/libjuliacli-debug.$(JL_MAJOR_MINOR_SHLIB_EXT)

# Build list of dependent libraries
LOADER_CFLAGS += -DDEP_LIBS="$(call dep_libs_gen,$(LIBGCC_DEPLIB)),\
					          $(call dep_libs_gen,$(LIBLLVM_DEPLIB)),\
                        	  $(call dep_libs_gen,$(LIBJULIACLI_DEPLIB))"

LOADER_DEBUG_CFLAGS += -DDEP_LIBS="$(call dep_libs_gen,$(LIBGCC_DEPLIB)),\
					               $(call dep_libs_gen,$(LIBLLVM_DEPLIB)),\
                        	       $(call dep_libs_gen,$(LIBJULIACLI_DEBUG_DEPLIB))"


ifeq ($(OS),WINNT)
LOADER_CFLAGS += -municode -mconsole -ffreestanding -nostdlib \
                    -fno-stack-check -fno-stack-protector -mno-stack-arg-probe
endif

#LOADER_LDFLAGS += -static
ifeq ($(OS),WINNT)
LOADER_LDFLAGS += -municode -mconsole -ffreestanding -nostdlib \
	               --disable-auto-import --disable-runtime-pseudo-reloc \
	               -lntdll -lkernel32 -lpsapi
else ifeq ($(OS),Linux)
LOADER_LDFLAGS += -ldl -rdynamic
endif

# Define special target for loader.o to take in these flags 
$(BUILDDIR)/loader.o: $(SRCDIR)/loader.c $(HEADERS)
	@$(call PRINT_CC, $(CC) $(LOADER_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(SHIPFLAGS) -c $< -o $@)
$(BUILDDIR)/loader.dbg.obj: $(SRCDIR)/loader.c $(HEADERS)
	@$(call PRINT_CC, $(CC) $(LOADER_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(DEBUGFLAGS) -c $< -o $@)


$(build_bindir)/julia$(EXE): $(BUILDDIR)/loader.o $(build_shlibdir)/libjuliacli.$(JL_MAJOR_MINOR_SHLIB_EXT)
	@$(call PRINT_LINK, $(CXXLD) $(LOADER_CFLAGS) $< -o $@ $(LOADER_LDFLAGS) $(LDFLAGS))

$(build_bindir)/julia-debug$(EXE): $(BUILDDIR)/loader.dbg.obj $(build_shlibdir)/libjuliacli.$(JL_MAJOR_MINOR_SHLIB_EXT)
	@$(call PRINT_LINK, $(CXXLD) $(LOADER_DEBUG_CFLAGS) $< -o $@ $(LOADER_LDFLAGS) $(LDFLAGS))


clean: | $(CLEAN_TARGETS)
	rm -f *.o *.dbg.obj
	rm -f $(build_bindir)/julia*

.PHONY: clean release debug julia-release julia-debug
