TOPDIR=/home/datom
FBTHRIFT_CPP=LD_LIBRARY_PATH=$(TOPDIR)/artifacts/lib $(TOPDIR)/artifacts/bin/thrift1
FBTHRIFT_CPP2_GEN_PATH = $(TOPDIR)/artifacts/lib/python2.7/dist-packages
FBTHRIFT_CPP2=LD_LIBRARY_PATH=$(TOPDIR)/artifacts/lib PYTHONPATH=$(FBTHRIFT_CPP2_GEN_PATH) python -mthrift_compiler.main --gen cpp2:process_in_event_base,namespace_cpp

all: infra infra-python

infra: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen commontypes.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen status.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen configtree.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen service.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen configapi.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen pbapi.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen volumeapi.thrift

infra-python: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen commontypes.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen status.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen configtree.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen service.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen configapi.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen pbapi.thrift

infra-java: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(FBTHRIFT_CPP) --gen java -out $(TOPDIR)/src/java/src/main/java commontypes.thrift
	$(FBTHRIFT_CPP) --gen java -out $(TOPDIR)/src/java/src/main/java status.thrift
	$(FBTHRIFT_CPP) --gen java -out $(TOPDIR)/src/java/src/main/java configtree.thrift
	$(FBTHRIFT_CPP) --gen java -out $(TOPDIR)/src/java/src/main/java service.thrift

infra-clean:
	rm -rf $(TOPDIR)/src/infra/gen/*
	rm -rf $(TOPDIR)/src/java/src/main/java/infra/gen/*
	rm -rf $(TOPDIR)/src/python/gen/*

clean: infra-clean
