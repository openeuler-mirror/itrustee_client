TARGET_LIB := libteec.so
TARGET_APP := teecd
TARGET_DIR := dist
LIBC_SEC   := libboundscheck
TARGET_LOG := tlogcat
TARGET_LIBSEC := libboundscheck.so
TARGET_AGENTD := agentd
WITH_CONFIDENTIAL_CONTAINER ?= true

COMMON_CFLAGS :=
ifeq ($(WITH_CONFIDENTIAL_CONTAINER), true)
	COMMON_CFLAGS += -DCONFIG_PATH_NAMED_SOCKET=\"/var/itrustee/teecd/teecd.sock\"

ifeq ($(TOOL_CHAIN),1)
	CC := aarch64-linux-gnu-gcc
	LD := aarch64-linux-gnu-ld
else
	CC := gcc
endif
export CC
export LD

all: $(TARGET_LIBSEC) $(TARGET_LIB) $(TARGET_APP) $(TARGET_LOG) $(TARGET_AGENTD)
	@cd $(LIBC_SEC) && $(MAKE) clean

install:
	[ -d "/usr/bin" ] && cp -f $(TARGET_DIR)/$(TARGET_APP) /usr/bin
	[ -d "/usr/bin" ] && cp -f $(TARGET_DIR)/$(TARGET_LOG) /usr/bin
	[ ! -d "/usr/lib64" ] && mkdir -p /usr/lib64 || true
	cp -f $(TARGET_DIR)/$(TARGET_LIB) /usr/lib64 # only for secgear
	[ -d "/lib64" ] && cp -f $(TARGET_DIR)/$(TARGET_LIB) /lib64
	[ -d "/lib64" ] && cp -f $(TARGET_DIR)/$(TARGET_LIBSEC) /lib64

install-container:
	[ -d "/usr/bin" ] && cp -f $(TARGET_DIR)/$(TARGET_LOG) /usr/bin
	[ -d "/usr/bin" ] && cp -f $(TARGET_DIR)/$(TARGET_AGENTD) /usr/bin
	[ ! -d "/usr/lib64" ] && mkdir -p /usr/lib64 || true
	cp -f $(TARGET_DIR)/$(TARGET_LIB) /usr/lib64 # only for secgear
	if [ -d "lib/aarch64-linux-gnu" ]; then \
		cp -f $(TARGET_DIR)/$(TARGET_LIB) /lib/aarch64-linux-gnu; \
		cp -f $(TARGET_DIR)/$(TARGET_LIBSEC) /lib/aarch64-linux-gnu; \
	elif [ -d "/usr/local/lib" ]; then \
		cp -f $(TARGET_DIR)/$(TARGET_LIB) /usr/local/lib; \
		cp -f $(TARGET_DIR)/$(TARGET_LIBSEC) /usr/local/lib; \
	else \
		echo "Not found ld library path"; \
	fi

LIB_CFLAGS := $(COMMON_CFLAGS) -DSEC_STORAGE_DATA_KUNPENG_PATH -DCONFIG_KUNPENG_PLATFORM -DCONFIG_AUTH_USERNAME -DDYNAMIC_TA_PATH=\"/var/itrustee/ta\"
LIB_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/authentication/
LIB_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIC -D_FORTIFY_SOURCE=2 -O2
LIB_LDFLAGS := -lboundscheck -Llibboundscheck/lib -shared

LIB_SOURCES := src/libteec_vendor/tee_client_api.c \
               src/libteec_vendor/tee_client_ext_api.c \
               src/libteec_vendor/tee_client_app_load.c \
               src/libteec_vendor/tee_client_socket.c \
               src/libteec_vendor/tee_load_sec_file.c \
               src/libteec_vendor/tee_session_pool.c

LIB_OBJECTS := $(LIB_SOURCES:.c=.o)

$(TARGET_LIBSEC):
	@echo "compile libboundscheck"
	@$(MAKE) -C $(LIBC_SEC)
	@echo "after compile libboundscheck"


$(TARGET_LIB):$(TARGET_LIBSEC) $(LIB_SOURCES)
	@echo "compile libteec.so"
	@$(CC) $(LIB_CFLAGS) -o $@ $(LIB_SOURCES) $(LIB_LDFLAGS)
	@mkdir -p $(TARGET_DIR)
	@mv libteec.so $(TARGET_DIR)
	@cp $(LIBC_SEC)/lib/libboundscheck.so $(TARGET_DIR)
	@echo "after compile libteec.so"


APP_CFLAGS := $(COMMON_CFLAGS) -DSEC_STORAGE_DATA_KUNPENG_PATH -D_GNU_SOURCE
APP_CFLAGS += -DCONFIG_KUNPENG_PLATFORM -DCONFIG_AUTH_USERNAME -DCONFIG_AGENT_FS -DCONFIG_AGENT_MISC -DCONFIG_AGENT_SECLOAD
APP_CFLAGS += -DDYNAMIC_DRV_DIR=\"/var/itrustee/tee_dynamic_drv/\" -DDYNAMIC_CRYPTO_DRV_DIR=\"/var/itrustee/tee_dynamic_drv/crypto/\" \
              -DDYNAMIC_SRV_DIR=\"/var/itrustee/tee_dynamic_srv/\" -DDYNAMIC_TA_PATH=\"/var/itrustee/ta/\"
APP_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/teecd/  -Isrc/authentication/ -Isrc/libteec_vendor/ -Isrc/common
APP_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
APP_LDFLAGS += -lboundscheck -Llibboundscheck/lib -lpthread -lcrypto

CFG_ENG = user
ifneq ($(strip $(CFG_ENG)), user)
APP_CFLAGS += -DDEF_ENG
endif

APP_SOURCES := src/teecd/teecd.c \
			   src/teecd/tee_agent.c \
			   src/teecd/tee_ca_daemon.c \
			   src/teecd/secfile_load_agent.c \
			   src/teecd/fs_work_agent.c \
			   src/teecd/misc_work_agent.c \
			   src/teecd/tee_ca_auth.c \
			   src/teecd/system_ca_auth.c \
			   src/teecd/tee_ca_hashfile.c \
			   src/authentication/tee_get_native_cert.c \
			   src/authentication/tee_auth_common.c \
			   src/teecd/tee_load_dynamic.c \
			   src/libteec_vendor/tee_load_sec_file.c \
			   src/common/dir.c \
			   src/common/tee_version_check.c

APP_OBJECTS := $(APP_SOURCES:.c=.o)

$(TARGET_APP): $(TARGET_LIBSEC) $(APP_SOURCES)
	@echo "compile teed"
	@$(CC) $(APP_CFLAGS) -o $@ $(APP_SOURCES) $(APP_LDFLAGS)
	@mkdir -p $(TARGET_DIR)
	@mv teecd $(TARGET_DIR)
	@echo "after compile teed"

#############################
## agentd
#############################
AGENTD_SOURCES := src/agentd/agentd.c \
				  src/teecd/tee_agent.c \
				  src/teecd/secfile_load_agent.c \
				  src/teecd/fs_work_agent.c \
				  src/teecd/misc_work_agent.c \
				  src/teecd/tee_load_dynamic.c \
				  src/libteec_vendor/tee_load_sec_file.c \
				  src/common/tee_version_check.c \
				  src/common/tee_custom_log.c

AGENTD_CFLAGS := $(COMMON_CFLAGS) -DSEC_STORAGE_DATA_KUNPENG_PATH -D_GNU_SOURCE -DCONFIG_KUNPENG_PLATFORM -DCONFIG_AUTH_USERNAME
AGENTD_CFLAGS += -DCONFIG_AGENT_FS -DCONFIG_AGENT_SECLOAD -DCONFIG_AGENT_MISC -DCONFIG_AGENTD
AGENTD_CFLAGS += -DDYNAMIC_TA_PATH=\"/var/itrustee/ta/\" -DCONFIG_CUSTOM_LOGGING=\"/var/log/agentd.log\"
AGENTD_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/teecd/
AGENTD_CFLAGS += -Isrc/authentication/ -Isrc/libteec_vendor/ -Isrc/common
AGENTD_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
AGENTD_LDFLAGS += $(LD_FLAGS) -lboundscheck -Llibboundscheck/lib -lpthread -lcrypto
ifneq($(strip $(CFG_ENG)), user)
AGENTD_CFLAGS += -DDEF_ENG
endif

AGENTD_OBJECTS := $(AGENTD_SOURCES:.c=.o)

$(TARGET_AGENTD): $(AGENTD_SOURCES)
	@echo "compile agentd"
	@$(CC) $(AGENTD_CFLAGS) -o $@ $(AGENTD_SOURCES) $(AGENTD_LDFLAGS)
	@mkdir -p $(TARGET_DIR)
	@mv agentd $(TARGET_DIR)
	@echo "after compile agentd"

#############################
## tlogcat
#############################
LOG_SOURCES := src/tlogcat/tarzip.c  \
	src/tlogcat/sys_syslog_cfg.c \
	src/tlogcat/tlogcat.c \
	src/common/tee_version_check.c

LOG_CFLAGS += -Werror -Wall -Wextra -DCONFIG_KUNPENG_PLATFORM -DCONFIG_AUTH_USERNAME
LOG_CFLAGS += -DTEE_LOG_PATH_BASE=\"/var/log\"

LOG_CFLAGS += -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2
LOG_CFLAGS += -Iinclude -Iinclude/cloud -Iext_include -Ilibboundscheck/include -Iinclude -Isrc/inc -Isrc/tlogcat/ -Isrc/common
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
