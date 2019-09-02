# sarah auto make file
# @Author grandhelmsman<desupport@grandhelmsman.com>

#complie
CC = gcc
#include directory
INCLUDE = .
#complie flags for devolep
CFLAGS = -O2 -Wall -fPIC -I../include/
#complile flags for products
FFLAGS = -O2 -Wall -fPIC -I../include/
#FFLAGS = -g -Wall -fPIC
#extension libs
ELIB = curl
LIBRARY_DIR = /usr/local/lib
INCLUDE_DIR = /usr/local/include
INSTALL_DIR = /usr/local/bin

SDK_NAME=SarahOS_Linux_64bit_SDK
RELEASE_DIR = ../build/$(SDK_NAME)
DEBUG_DIR = ../build/debug
CONFIG_DIR = ../config

# check and enable the ghost version
ifeq ($(MAKECMDGOALS),ghost)
    FFLAGS += -D _SARAH_GHOST_UDISK_ENABLE -D _SARAH_ASSERT_PARENT_PROCESS
endif


OBJECT = node.o cpu.o ram.o disk.o network.o process.o hardware.o utils.o executor.o script_bash.o lib_executor.o lib_april.o april.o script_lua.o script_sarah.o cel_stdio.o cel_string.o cel_link.o cel_array.o cel_md5.o cel_hash.o cel_math.o cel_hashmap.o cJSON.o cJSON_Utils.o mbedtls_platform.o mbedtls_platform_util.o mbedtls_sha1.o mbedtls_sha256.o mbedtls_sha512.o mbedtls_base64.o app_collector.o app_executor.o app_updater.o app_smartdisk.o
OBJFUN = node.o cpu.o ram.o disk.o network.o process.o hardware.o utils.o executor.o script_bash.o lib_executor.o lib_april.o april.o script_lua.o script_sarah.o cel_stdio.c cel_string.o cel_link.o cel_array.o cel_md5.o cel_hash.o cel_math.o cel_hashmap.o cJSON.o cJSON_Utils.o mbedtls_platform.o mbedtls_platform_util.o mbedtls_sha1.o mbedtls_sha256.o mbedtls_sha512.o mbedtls_base64.o
OBJAPR = utils.o lib_april.o april.o script_lua.o cel_stdio.o cel_md5.o cJSON.o cJSON_Utils.o mbedtls_platform.o mbedtls_platform_util.o mbedtls_sha1.o mbedtls_sha256.o mbedtls_sha512.o mbedtls_base64.o

all: sarah-booter sarah-shared sarah-executor sarah-april
ghost: sarah-booter

# @Note we compile the application with -no-pie option
# to create the LSB executable application output
sarah-booter: $(OBJECT) sarah.o
	$(CC) -no-pie $(FFLAGS) $(OBJECT) sarah.o ../lib/libcurl.a ../lib/liblua.a -lpthread -lm -ldl -o sarah-booter

sarah-shared: $(OBJFUN)
	$(CC) -no-pie $(FFLAGS) -shared $(OBJFUN) ../lib/libcurl.a ../lib/liblua.a -lpthread -lm -ldl -o libsarah.so

sarah-executor: $(OBJFUN) sarah_executor.o
	$(CC) -no-pie $(FFLAGS) $(OBJFUN) sarah_executor.o ../lib/libcurl.a ../lib/liblua.a -lpthread -lm -ldl -o sarah-script

sarah-april: $(OBJAPR) aprilc.o
	$(CC) -no-pie $(FFLAGS) $(OBJFUN) aprilc.o ../lib/libcurl.a ../lib/liblua.a -lpthread -lm -ldl -o april


sarah.o: sarah.c
sarah_executor.o: sarah_executor.c
node.o: node.c
cpu.o: cel_link.c cpu.c
ram.o: ram.c
disk.o: cel_link.c disk.c
network.o: cel_link.c network.c
process.o: cel_link.c process.c
hardware.o: hardware.c
utils.o: utils.c
script_bash.o: script_bash.c
lib_executor.o: lib_executor.c
lib_april.o: lib_april.c
april.o: april.c
aprilc.o: aprilc.c
script_lua.o: utils.c script_lua.c
script_sarah.o: script_sarah.c
executor.o: executor.c
cel_stdio.o: cel_stdio.c
cel_string.o: cel_string.c
cel_link.o: cel_link.c
cel_array.o: cel_array.c
cel_md5.o: cel_md5.c
cel_hash.o: cel_hash.c
cel_math.o: cel_math.c
cel_hashmap.o: cel_hashmap.c
cJSON.o: cJSON.c
cJSON_Utils.o: cJSON_Utils.c
mbedtls_platform.o: mbedtls_platform.c
mbedtls_platform_util.o: mbedtls_platform_util.c
mbedtls_sha1.o: mbedtls_sha1.c
mbedtls_sha256.o: mbedtls_sha256.c
mbedtls_sha512.o: mbedtls_sha512.c
mbedtls_base64.o: mbedtls_base64.c
app_collector.o: cel_link.c app_collector.c
app_executor.o: cel_link.c app_executor.c
app_updater.o: app_updater.c
app_smartdisk.o: cel_link.c app_smartdisk.c


install:
	cp libsarah.so $(LIBRARY_DIR)/
	chmod 644 $(LIBRARY_DIR)/libsarah.so
	cp sarah-script /usr/local/bin/
	chmod 755 /usr/local/bin/sarah-script
	cp april /usr/local/bin/
	chmod 755 /usr/local/bin/april

# clean all the object files.
.PHONY: clean
clean:
	find $(BUILD_DIR) -name \*.so | xargs rm -f
	find $(BUILD_DIR) -name \*.o  | xargs rm -f
	find $(BUILD_DIR) -name \*.a  | xargs rm -f
	find $(BUILD_DIR) -name sarah-booter  | xargs rm -f
	find $(BUILD_DIR) -name sarah-script | xargs rm -f
	find $(BUILD_DIR) -name april | xargs rm -f

# generate the release package
release: sarah-booter
	@if [ ! -d $(RELEASE_DIR) ];\
	    then\
        mkdir -p $(RELEASE_DIR); \
	fi
	@cp -f sarah-booter $(RELEASE_DIR)/;\
	chmod 755 $(RELEASE_DIR)/sarah-booter;\
	cp -f $(CONFIG_DIR)/sarah $(RELEASE_DIR)/;\
	chmod 755 $(RELEASE_DIR)/sarah;\
	cp -f $(CONFIG_DIR)/install.sh $(RELEASE_DIR)/;\
	chmod 755 $(RELEASE_DIR)/install.sh;\
	cp -f $(CONFIG_DIR)/uninstall.sh $(RELEASE_DIR)/;\
	chmod 755 $(RELEASE_DIR)/uninstall.sh;\
	cp -f $(CONFIG_DIR)/README.md $(RELEASE_DIR)/;\
	cp -f $(CONFIG_DIR)/config.json $(RELEASE_DIR)/;\
	chmod 755 $(RELEASE_DIR)/config.json;\
    sed -i "s/{build}/release/g" $(RELEASE_DIR)/config.json; \
	zip ../build/$(SDK_NAME).zip $(RELEASE_DIR)/*;\
	tar -zcvf ../build/$(SDK_NAME).tar.gz $(RELEASE_DIR)/*;\
    echo "Release package generated: "; \
    echo "|-position: $(RELEASE_DIR)"; \
    echo "|-zip pacakge: ../build/$(SDK_NAME).zip"; \
    echo "|-tar pacakge: ../build/$(SDK_NAME).tar.gz"; \
    echo "|-Documentation: $(RELEASE_DIR)/README.md";

# generate the debug package
debug: sarah-booter
	@if [ ! -d $(DEBUG_DIR) ];\
	    then\
        mkdir -p $(DEBUG_DIR); \
	fi
	@cp -f sarah-booter $(DEBUG_DIR)/;\
	chmod 755 $(DEBUG_DIR)/sarah-booter;\
	cp -f $(CONFIG_DIR)/sarah $(DEBUG_DIR)/;\
	chmod 755 $(DEBUG_DIR)/sarah;\
	cp -f $(CONFIG_DIR)/install.sh $(DEBUG_DIR)/;\
	chmod 755 $(DEBUG_DIR)/install.sh;\
	cp -f $(CONFIG_DIR)/uninstall.sh $(DEBUG_DIR)/;\
	chmod 755 $(DEBUG_DIR)/uninstall.sh;\
	cp -f $(CONFIG_DIR)/README.md $(DEBUG_DIR)/;\
	cp -f $(CONFIG_DIR)/config.json $(DEBUG_DIR)/;\
	chmod 755 $(DEBUG_DIR)/config.json;\
    sed -i "s/{build}/debug/g" $(DEBUG_DIR)/config.json; \
    echo "Release package generated: "; \
    echo "|-position: $(DEBUG_DIR)"; \
    echo "|-Documentation: $(DEBUG_DIR)/README.md";
