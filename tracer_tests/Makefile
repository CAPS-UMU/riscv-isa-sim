# -*- Makefile -*-

benchmarks:=test00 \
	test01 test02 test03 test04 test05 test06 test07 test08 test09 \
	a01-far-atomic-friendly a02-near-atomic-friendly 

all: $(benchmarks)
.PHONY: $(benchmarks)
$(benchmarks):
	make -C $@

clean:
	for i in $(benchmarks) ; do make -C $$i $@ ; done

