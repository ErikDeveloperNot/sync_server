##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=registrationServer
ConfigurationName      :=Debug
WorkspacePath          :=/Users/user1/udemy/CPP/UdemyCPP
ProjectPath            :=/Users/user1/udemy/CPP/UdemyCPP/registrationServer
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=User One
Date                   :=01/10/2019
CodeLitePath           :="/Users/user1/Library/Application Support/CodeLite"
LinkerName             :=/usr/bin/clang++
SharedObjectLinkerName :=/usr/bin/clang++ -dynamiclib -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="registrationServer.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)/opt/openssl/openssl-1.1.1c_install/include $(IncludeSwitch)/Users/user1/udemy/CPP/UdemyCPP/registrationServer_psql/include $(IncludeSwitch)/Users/user1/udemy/CPP/UdemyCPP/jsonP_dyn 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)ecpg $(LibrarySwitch)ecpg_compat $(LibrarySwitch)pgcommon $(LibrarySwitch)pgfeutils $(LibrarySwitch)pgport $(LibrarySwitch)pgtypes $(LibrarySwitch)pq $(LibrarySwitch)crypto $(LibrarySwitch)ssl $(LibrarySwitch)jsonP_dyn 
ArLibs                 :=  "libecpg.a" "libecpg_compat.a" "libpgcommon.a" "libpgfeutils.a" "libpgport.a" "libpgtypes.a" "libpq.a" "libcrypto" "libssl" "jsonP_dyn" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)/opt/openssl/openssl-1.1.1c_install/lib $(LibraryPathSwitch)/Users/user1/udemy/CPP/UdemyCPP/registrationServer_psql/lib $(LibraryPathSwitch)/Users/user1/udemy/CPP/UdemyCPP/jsonP_dyn/Debug 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/clang++
CC       := /usr/bin/clang
CXXFLAGS := -g -std=c++11 -Wall -g -O0 -std=c++11 -Wall $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/Applications/codelite.app/Contents/SharedSupport/
Objects0=$(IntermediateDirectory)/main.cpp$(ObjectSuffix) $(IntermediateDirectory)/server.cpp$(ObjectSuffix) $(IntermediateDirectory)/sync_handler.cpp$(ObjectSuffix) $(IntermediateDirectory)/json_parser_exception.cpp$(ObjectSuffix) $(IntermediateDirectory)/data_store_connection.cpp$(ObjectSuffix) $(IntermediateDirectory)/json_parser.cpp$(ObjectSuffix) $(IntermediateDirectory)/Config.cpp$(ObjectSuffix) $(IntermediateDirectory)/register_server_exception.cpp$(ObjectSuffix) $(IntermediateDirectory)/config_http.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Debug || $(MakeDirCommand) ./Debug


$(IntermediateDirectory)/.d:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/main.cpp$(ObjectSuffix): main.cpp $(IntermediateDirectory)/main.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/main.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/main.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/main.cpp$(DependSuffix): main.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/main.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/main.cpp$(DependSuffix) -MM main.cpp

$(IntermediateDirectory)/main.cpp$(PreprocessSuffix): main.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/main.cpp$(PreprocessSuffix) main.cpp

$(IntermediateDirectory)/server.cpp$(ObjectSuffix): server.cpp $(IntermediateDirectory)/server.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/server.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/server.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/server.cpp$(DependSuffix): server.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/server.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/server.cpp$(DependSuffix) -MM server.cpp

$(IntermediateDirectory)/server.cpp$(PreprocessSuffix): server.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/server.cpp$(PreprocessSuffix) server.cpp

$(IntermediateDirectory)/sync_handler.cpp$(ObjectSuffix): sync_handler.cpp $(IntermediateDirectory)/sync_handler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/sync_handler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/sync_handler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/sync_handler.cpp$(DependSuffix): sync_handler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/sync_handler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/sync_handler.cpp$(DependSuffix) -MM sync_handler.cpp

$(IntermediateDirectory)/sync_handler.cpp$(PreprocessSuffix): sync_handler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/sync_handler.cpp$(PreprocessSuffix) sync_handler.cpp

$(IntermediateDirectory)/json_parser_exception.cpp$(ObjectSuffix): json_parser_exception.cpp $(IntermediateDirectory)/json_parser_exception.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/json_parser_exception.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/json_parser_exception.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/json_parser_exception.cpp$(DependSuffix): json_parser_exception.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/json_parser_exception.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/json_parser_exception.cpp$(DependSuffix) -MM json_parser_exception.cpp

$(IntermediateDirectory)/json_parser_exception.cpp$(PreprocessSuffix): json_parser_exception.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/json_parser_exception.cpp$(PreprocessSuffix) json_parser_exception.cpp

$(IntermediateDirectory)/data_store_connection.cpp$(ObjectSuffix): data_store_connection.cpp $(IntermediateDirectory)/data_store_connection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/data_store_connection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/data_store_connection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/data_store_connection.cpp$(DependSuffix): data_store_connection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/data_store_connection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/data_store_connection.cpp$(DependSuffix) -MM data_store_connection.cpp

$(IntermediateDirectory)/data_store_connection.cpp$(PreprocessSuffix): data_store_connection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/data_store_connection.cpp$(PreprocessSuffix) data_store_connection.cpp

$(IntermediateDirectory)/json_parser.cpp$(ObjectSuffix): json_parser.cpp $(IntermediateDirectory)/json_parser.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/json_parser.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/json_parser.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/json_parser.cpp$(DependSuffix): json_parser.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/json_parser.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/json_parser.cpp$(DependSuffix) -MM json_parser.cpp

$(IntermediateDirectory)/json_parser.cpp$(PreprocessSuffix): json_parser.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/json_parser.cpp$(PreprocessSuffix) json_parser.cpp

$(IntermediateDirectory)/Config.cpp$(ObjectSuffix): Config.cpp $(IntermediateDirectory)/Config.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/Config.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/Config.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/Config.cpp$(DependSuffix): Config.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/Config.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/Config.cpp$(DependSuffix) -MM Config.cpp

$(IntermediateDirectory)/Config.cpp$(PreprocessSuffix): Config.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/Config.cpp$(PreprocessSuffix) Config.cpp

$(IntermediateDirectory)/register_server_exception.cpp$(ObjectSuffix): register_server_exception.cpp $(IntermediateDirectory)/register_server_exception.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/register_server_exception.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/register_server_exception.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/register_server_exception.cpp$(DependSuffix): register_server_exception.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/register_server_exception.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/register_server_exception.cpp$(DependSuffix) -MM register_server_exception.cpp

$(IntermediateDirectory)/register_server_exception.cpp$(PreprocessSuffix): register_server_exception.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/register_server_exception.cpp$(PreprocessSuffix) register_server_exception.cpp

$(IntermediateDirectory)/config_http.cpp$(ObjectSuffix): config_http.cpp $(IntermediateDirectory)/config_http.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/Users/user1/udemy/CPP/UdemyCPP/registrationServer/config_http.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/config_http.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/config_http.cpp$(DependSuffix): config_http.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/config_http.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/config_http.cpp$(DependSuffix) -MM config_http.cpp

$(IntermediateDirectory)/config_http.cpp$(PreprocessSuffix): config_http.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/config_http.cpp$(PreprocessSuffix) config_http.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


