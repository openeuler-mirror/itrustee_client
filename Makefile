TARGET_LIB := libteec.so
TARGET_APP := teecd
TARGET_DIR := dist
LIBC_SEC   := libboundscheck
TARGET_LOG := tlogcat
TARGET_LIBSEC := libboundscheck.so

LIB_CFLAGS := -DDYNAMIC_TA_PATH=\"/var/itrustee/ta/\"
LIB_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc  -Isrc/authentication/
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
               src/libteec_vendor/tee_load_sec_file.c \
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


APP_CFLAGS := -D_GNU_SOURCE -DCONFIG_AGENT_FS
APP_CFLAGS += -DDYNAMIC_DRV_DIR=\"/var/itrustee/tee_dynamic_drv/\" -DDYNAMIC_CRYPTO_DRV_DIR=\"/var/itrustee/tee_dynamic_drv/crypto/\" \
              -DDYNAMIC_SRV_DIR=\"/var/itrustee/tee_dynamic_srv/\" -DDYNAMIC_TA_PATH=\"/var/itrustee/ta/\"
APP_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/teecd/  -Isrc/authentication/ -Isrc/libteec_vendor/
APP_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
APP_LDFLAGS += -lboundscheck -Llibboundscheck/lib -lpthread -lcrypto

CFG_ENG = user
ifneq ($(strip $(CFG_ENG)), user)
APP_CFLAGS += -DDEF_ENG
endif

APP_SOURCES := src/teecd/tee_agent.c \
			   src/teecd/tee_ca_daemon.c \
			   src/teecd/secfile_load_agent.c \
			   src/teecd/fs_work_agent.c \
			   src/teecd/misc_work_agent.c \
			   src/teecd/tee_ca_auth.c \
			   src/teecd/system_ca_auth.c \
			   src/authentication/tee_get_native_cert.c \
			   src/authentication/tee_auth_common.c \
			   src/teecd/tee_load_dynamic.c \
			   src/libteec_vendor/tee_load_sec_file.c

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
LOG_SOURCES := src/tlogcat/tarzip.c  \
	src/tlogcat/sys_syslog_cfg.c \
	src/tlogcat/tlogcat.c

LOG_CFLAGS += -Werror -Wall -Wextra
LOG_CFLAGS += -DTEE_LOG_PATH_BASE=\"/var/log\"

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
