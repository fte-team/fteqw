#Note: This makefile builds the iqm tool without any fte dependancies
#      This means no .mdl export nor extended model format support (read: no gltf/glb import)

CXXFLAGS= -O3 -fomit-frame-pointer
override CXXFLAGS+= -Wall -fsigned-char

IQM_OBJS= \
	iqm.o
UPGRADE_OBJS= \
	upgrade.o

default: all

all: iqm #upgrade

clean:
	-$(RM) $(IQM_OBJS) $(UPGRADE_OBJS) iqm upgrade

iqm: $(IQM_OBJS)
	$(CXX) $(CXXFLAGS) -o iqm $(IQM_OBJS)
upgrade: $(UPGRADE_OBJS)
	$(CXX) $(CXXFLAGS) -o upgrade $(UPGRADE_OBJS)

%.o : %.cpp
	$(CXX) -o $@ -c $<
