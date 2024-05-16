BITS = 64
ODIR  = obj64
tmp  := $(shell mkdir -p $(ODIR))

CXX      = g++
CXXFLAGS = -MMD -ggdb -O3 -std=gnu++17 -m$(BITS)
LDFLAGS	 = -m$(BITS) -lpthread -lrt

CXXFILES = test

# builds the executable
TARGET = $(ODIR)/test

# creates .o files
OFILES = $(patsubst %, $(ODIR)/%.o, $(CXXFILES))

# for clean
DFILES = $(patsubst %.o, %.d, $(OFILES))

# to build executable
all: $(TARGET)

# clean
clean:
	@echo cleaning up...
	@rm -rf $(ODIR)

# build an .o file from a .cc file
$(ODIR)/%.o: %.cc
	@echo [CXX] $< "-->" $@
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

# build .o files
$(TARGET): $(OFILES)
	@echo [LD] $^ "-->" $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

# all and clean are not targets
.PHONY: all clean


-include $(DFILES)


