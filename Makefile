TARGET_LIB := libteec.so
TARGET_APP := teecd
TARGET_DIR := dist
LIBC_SEC   := libboundscheck
TARGET_LOG := tlogcat
TARGET_LIBSEC := libboundscheck.so

LIB_CFLAGS := -DSEC_STORAGE_DATA_KUNPENG_PATH -DSECURITY_AUTH_ENHANCE
LIB_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/teecd/  -Isrc/authentication/
LIB_CFLAGS += -lboundscheck -Llibboundscheck/lib -shared
LIB_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIC -D_FORTIFY_SOURCE=2 -O2

ifeq ($(TOOL_CHAIN),1)
	CC := aarch64-linux-gnu-gcc
	LD := aarch64-linux-gnu-ld
else
	CC := gcc
endif
export CC
export LD

LIB_SOURCES := src/libteec_vendor/tee_client_api.c \
               src/libteec_vendor/tee_client_ext_api.c \
               src/libteec_vendor/tee_client_app_load.c \
               src/libteec_vendor/tee_client_socket.c \
               src/teecd/secfile_load_agent.c \
               src/libteec_vendor/tee_session_pool.c

LIB_OBJECTS := $(LIB_SOURCES:.c=.o)

all: $(TARGET_LIBSEC) $(TARGET_LIB) $(TARGET_APP) $(TARGET_LOG)
	@cd $(LIBC_SEC) && $(MAKE) clean

$(TARGET_LIBSEC):
	@echo "compile libboundscheck"
	@$(MAKE) -C $(LIBC_SEC)
	@echo "after compile libboundscheck"


$(TARGET_LIB):$(TARGET_LIBSEC) $(LIB_SOURCES)
	@echo "compile libteec.so"
	@$(CC) $(LIB_CFLAGS) $(LIB_CFLAGS) -o $@ $(LIB_SOURCES)
	@mkdir -p $(TARGET_DIR)
	@mv libteec.so $(TARGET_DIR)
	@cp $(LIBC_SEC)/lib/libboundscheck.so $(TARGET_DIR)
	@echo "after compile libteec.so"


APP_CFLAGS := -DSEC_STORAGE_DATA_KUNPENG_PATH -D_GNU_SOURCE -DSECURITY_AUTH_ENHANCE -DCONFIG_AGENT_FS
APP_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/teecd/  -Isrc/authentication/
APP_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
APP_LDFLAGS += -lboundscheck -Llibboundscheck/lib -lpthread -lcrypto

APP_SOURCES := src/teecd/tee_agent.c \
			   src/teecd/tee_ca_daemon.c \
			   src/teecd/secfile_load_agent.c \
			   src/teecd/fs_work_agent.c \
			   src/teecd/misc_work_agent.c \
			   src/teecd/tee_ca_auth.c \
			   src/teecd/system_ca_auth.c \
			   src/authentication/tee_get_native_cert.c \
			   src/authentication/tee_auth_common.c

APP_OBJECTS := $(APP_SOURCES:.c=.o)

$(TARGET_APP): $(TARGET_LIBSEC) $(APP_SOURCES)
	@echo "compile teecd"
	@$(CC) $(APP_CFLAGS) -o $@ $(APP_SOURCES) $(APP_LDFLAGS)
	@mkdir -p $(TARGET_DIR)
	@mv teecd $(TARGET_DIR)
	@echo "after compile teecd"

#############################
## tlogcat
#############################
LOG_SOURCES := src/tlogcat/read_ktrace.c \
	src/tlogcat/tarzip.c  \
	src/tlogcat/tlogcat.c

LOG_CFLAGS += -Werror -Wall -Wextra -DTLOGCAT_SYS_LOG
LOG_CFLAGS += -DTEE_KTRACE_DUMP
LOG_CFLAGS += -DLOG_PATH_TEE=\"/var/log/tee/\"
LOG_CFLAGS += -DLOG_PATH_BASE=\"/var/log/\"
LOG_CFLAGS += -DLOG_TMPPATH_TEE=\"/var/log/tee/_tmp/\"
LOG_CFLAGS += -DAID_SYSTEM=0

LOG_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
LOG_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/tlogcat/
LOG_LDFLAGS += -lboundscheck -Llibboundscheck/lib -lz
LOG_OBJECTS := $(LOG_SOURCES:.c=.o)
$(TARGET_LOG): $(TARGET_LIBSEC) $(LOG_SOURCES)
	@echo "compile tlogcat"
	@$(CC) $(LOG_CFLAGS) -o $@ $(LOG_SOURCES) $(LOG_LDFLAGS)
	@mkdir -p $(TARGET_DIR)
	@mv tlogcat $(TARGET_DIR)
	@echo "after compile tlogcat"

clean:
	@cd $(LIBC_SEC) && $(MAKE) clean
	@rm -rf $(TARGET_DIR)
