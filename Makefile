include $(API_ROOT)/ComDrv.mk

INCLUDE = $(COMDRV_INCL) -I.

# GCOV flags + libs
#CXXFLAGS += -fprofile-arcs -ftest-coverage -O0 -fPIC
CXXFLAGS += -std=c++11 -ggdb -rdynamic -O3 
#LIBS	= $(COMDRV_LIBS) $(LINKLIB) -pthread -lgcov

WRAPPER = snap7.cpp
LIBS	= $(COMDRV_LIBS) $(LINKLIB) -pthread -lsnap7

OBJS = $(COMDRV_OBJS)

DRV_NAME = WCCOARAMS7200
MYOBJS = RAMS7200Drv.o \
	RAMS7200HWMapper.o \
	RAMS7200HWService.o \
	RAMS7200Resources.o \
	RAMS7200LibFacade.o \
	RAMS7200Encryption.o \
	RAMS7200Main.o

define INSTALL_BODY
	@if [[ -n "$(PVSS_PROJ_PATH)" ]]; then \
	; \
	fi;
endef

COMMON_SOURCE = $(wildcard Common/*.cxx)
COMMON_TYPES = $(COMMON_SOURCE:.cxx=.o)

TRANSFORMATIONS_SOURCE = $(wildcard Transformations/*.cxx)
TRANSFORMATIONS = $(TRANSFORMATIONS_SOURCE:.cxx=.o)

WCCOARAMS7200: $(MYOBJS) $(COMMON_TYPES) $(TRANSFORMATIONS) Common/Constants.cxx
	@rm -f addVerInfo.o
	@$(MAKE) addVerInfo.o
	$(LINK_CMD) -o $(DRV_NAME) *.o $(OBJS) ./$(WRAPPER) $(LIBS)


installLibs:
	sudo yum install --assumeyes cyrus-sasl-gssapi cyrus-sasl-devel boost* cmake openssl-devel
	git submodule update --init --recursive
	#cd ./libs/librdS7200 && ./configure && make && sudo make install
	#cd ./libs/cppS7200 && mkdir -p build && cd build && cmake .. && make && sudo make install
	

install: WCCOARAMS7200
ifdef PVSS_PROJ_PATH
	@echo
	@echo "***************************************************************************************"
	@echo "Installing binaries in: $(PVSS_PROJ_PATH)"
	@echo "***************************************************************************************"
	@echo
	@cp -f $(DRV_NAME) $(PVSS_PROJ_PATH)/$(DRV_NAME)
else
	@echo
	@echo "*****************************************************************************************"
	@echo "ERROR !!!!!!!"
	@echo "You have to specified installation dir: PVSS_PROJ_PATH=<path_to_pvss_project_bin_folder>"
	@echo "*****************************************************************************************"
	@echo
endif

restart:
	pkill -9 -u $(USER) -f /$(DRV_NAME)

update: clean install restart

clean:
	@rm -f *.o $(DRV_NAME) WCCOARAMS7200

docs:
	doxygen Doxyfile

addVerInfo.cxx: $(API_ROOT)/include/Basics/Utilities/addVerInfo.hxx
	@cp -f $(API_ROOT)/include/Basics/Utilities/addVerInfo.hxx addVerInfo.cxx

addVerInfo.o: $(OFILES) addVerInfo.cxx

