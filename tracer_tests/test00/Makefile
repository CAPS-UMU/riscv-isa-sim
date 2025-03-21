benchmark=test00

as_riscv=riscv64-unknown-linux-gnu-as
ld_riscv=riscv64-unknown-linux-gnu-ld

all: $(benchmark).riscv64gcv

$(benchmark).riscv64gcv: $(benchmark).s
	$(as_riscv) -march=rv64gcv -o $@.o $<
	$(ld_riscv) -o $@ $@.o

%.objdump: %
	riscv64-unknown-linux-gnu-objdump -S $< > $@

clean:
	rm -f $(benchmark).riscv64gcv.o $(benchmark).riscv64gcv
