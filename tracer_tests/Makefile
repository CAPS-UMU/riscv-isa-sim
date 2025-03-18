# -*- Makefile -*-

benchmarks:=test01 test02 test03 test04 test05 test06

all: $(benchmarks)
.PHONY: $(benchmarks)
$(benchmarks):
	make -C $@

clean:
	for i in $(benchmarks) ; do make -C $$i $@ ; done

