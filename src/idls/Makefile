TOPDIR=/home/nbayyana/playground/cpp/test_fbthrift
THRIFT=$(TOPDIR)/thrift-0.9.3/compiler/cpp/thrift
FBTHRIFT_CPP=$(TOPDIR)/artifacts/bin/thrift1
FBTHRIFT_CPP2_GEN_PATH = $(TOPDIR)/artifacts/bin
FBTHRIFT_CPP2=PYTHONPATH=$(FBTHRIFT_CPP2_GEN_PATH) python -mthrift_compiler.main --gen cpp2:process_in_event_base,namespace_cpp

#PYTHONPATH=$(FBTHRIFT_CPP2_GEN_PATH) python -mthrift_compiler.main --gen cpp2:json -o $(TOPDIR)/src/infra/gen service.thrift
all: infra infra-python

infra: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen commontypes.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen status.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen configtree.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen service.thrift
	$(FBTHRIFT_CPP2) -o $(TOPDIR)/src/infra/gen configapi.thrift
	#$(FBTHRIFT_CPP) --gen cpp -out $(TOPDIR)/src/infra/gen service.thrift
	#PYTHONPATH=$(FBTHRIFT_CPP2_GEN_PATH) python -mthrift_compiler.main --gen cpp2:process_in_event_base,json -o $(TOPDIR)/src/infra/gen configapi.thrift

infra-python: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen commontypes.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen status.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen configtree.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen service.thrift
	$(FBTHRIFT_CPP) --gen py -out $(TOPDIR)/src/python/gen configapi.thrift

infra-java: commontypes.thrift status.thrift configtree.thrift service.thrift
	$(THRIFT) --gen java -out $(TOPDIR)/src/java/src/main/java commontypes.thrift
	$(THRIFT) --gen java -out $(TOPDIR)/src/java/src/main/java status.thrift
	$(THRIFT) --gen java -out $(TOPDIR)/src/java/src/main/java configtree.thrift
	$(THRIFT) --gen java -out $(TOPDIR)/src/java/src/main/java service.thrift

infra-clean:
	rm -rf $(TOPDIR)/src/infra/gen/*
	rm -rf $(TOPDIR)/src/java/src/main/java/infra/gen/*
	rm -rf $(TOPDIR)/src/python/gen/*

clean: infra-clean
