#############################
# RedHate 2024
#############################

TARGET		=   vc4a
OBJS		=  main.o
LDLIBS		=  -lGL -lSDL2main -lSDL2 -lz -lpthread


CC			=  gcc
CXX			=  g++
LD			=  gcc
MV			=  mv
CP			=  cp
ECHO		=  echo
RM 			=  rm
AR			=  ar
RANLIB   	=  ranlib
STRIP		=  strip
UNAME		=  $(shell uname)

INCLUDES	?= -I/usr/include
LIBS		?= -L/usr/lib

CFLAGS   	= -Wall -g -O2 $(INCLUDES) $(LIBS) -fPIC -pie
CXXFLAGS 	= -Wall -g -O2 $(INCLUDES) $(LIBS) -fPIC -pie

#WARNINGS	:= -w

## colors for fun
RED1		= \033[1;31m
RED2		= \033[0;31m
NOCOLOR		= \033[0m

.PHONY: all run clean

all: $(ASSETS) $(OBJS) $(TARGET) $(TARGET_LIB)

run:  $(ASSETS) $(OBJS) $(TARGET) $(TARGET_LIB) 
	@./$(TARGET)

clean: 
	@printf "$(RED1)[CLEANING]$(NOCOLOR)\n" 
	@rm $(OBJS) $(TARGET) $(TARGET_LIB) $(ASSETS)

list:
	@echo $(INCLUDES) $(LIBS)

%.h: %.png
	@printf "$(RED1)[PNG2H]$(NOCOLOR) $(notdir $(basename $<)).h\n" 
	@$(PNG2H) $< $(notdir $(basename $<)) $(KEY)
	@mv $(notdir $(basename $<)).h textures/$(notdir $(basename $<)).h

%.o: %.cpp
	@printf "$(RED1)[CXX]$(NOCOLOR) $(notdir $(basename $<)).o\n" 
	@$(CXX) $(WARNINGS) -c $< $(CXXFLAGS) -o $(basename $<).o 

%.o: %.cxx
	@printf "$(RED1)[CXX]$(NOCOLOR) $(notdir $(basename $<)).o\n" 
	@$(CXX) $(WARNINGS) -c $< $(CFLAGS) -o $(basename $<).o 

%.o: %.c
	@printf "$(RED1)[CC]$(NOCOLOR) $(notdir $(basename $<)).o\n" 
	@$(CC) $(WARNINGS) -c $< $(CFLAGS) -o $(basename $<).o 

%.a:
	@printf "$(RED1)[CC]$(NOCOLOR) $(basename $(TARGET_LIB)).a\n" 
	@$(AR) -cru $(basename $(TARGET_LIB)).a $(OBJS)

$(TARGET): $(ASSETS) $(OBJS)
	@printf "$(RED1)[CXX]$(NOCOLOR) $(TARGET) - Platform: $(UNAME)\n" 
	@$(CXX) $(WARNINGS)  $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LDLIBS) `sdl2-config --cflags --libs` -Wl,-E
	@chmod a+x $(TARGET)
	@$(STRIP) -s $(TARGET)
