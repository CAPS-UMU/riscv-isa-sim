# Spike RISC-V ISA Simulator with tracer for gems4caps

This is a fork of Spike that can generate traces for gems4caps. It's requirements and build instructions are the same as the original Spike.

It adds the following command line options to Spike:

 - `--log-g4trace`: Enable the generation of gems4caps traces.
 - `--log-g4trace-dest`: Specify the destination of the trace. A directory will be created with the given path.
 - `--log-g4trace-debug`: Enable debug comments in the generated traces.

Some other options are added to control the format of the trace and for debugging. Run with `--help` for more information.

The build procedure is the same as upstream Spike. You can read or use the `build-riscv-tracer` script to build the tracer and the required riscv-spike-sdk in a way that has already been tested. Note that building riscv-spike-sdk is sometimes tedious because some of the involved repositories tend to fail temporarily.
 
Programs can be simulated (and traced) in all the same ways as with upstream Spike. The script `spike-run-fs` can be used to run a program using full system simulation using the kernel and initrd built with buildroot by riscv-spike-sdk (see buildroot documentation for possible customizations). An additional temporary initrd will be created with the program to be simulated and some supporting files. See `spike-run-fs --help` and the source of the script for more information.

The traced programs are expected to be annotated using the hint instructions defined in `g4tracer-interface/g4tracer-interface.h`. At least `pg4tracer_start_tracing`, `g4tracer_start_ROI` and `g4tracer_end_ROI` should be called by each thread that needs to be traced. Syncronization needs to be annotated using the `g4trace_*_sm_* funtions`. The resulting trace will only contain traces for user level threads that have called `g4tracer_start_tracing`. 
 
Tracing of priviledged (OS) code is not supported. Priviledged instructions will be filtered.

The tracer works per OS thread. Threads are identified by the `tp` register and `satp` csr. This is expected to work reliably for user level threads as long as the phisical address of the root of the page table of the traced process does not change during execution. Several processes can be traced simultaneously, but there is no information in the trace about which thread belongs to which proccess. 

Thread binding and the number of processors used by spike is mostly irrelevant for the traces (except that different scheduling by the OS may produce different traces when syncronization is involved).
 
Test programs are in tracer_test. 

Examples of use:

```
   # The spike-run-fs script  allows to do full system simulation and passing any argument to Spike. It should work with upstream Spike also.
   ./spike-run-fs -s--log-g4trace -s--log-g4trace-dest=./trace-output -- ./tracer_tests/test02/test02.gcc.riscv64gcv
```

```
   # The trace-benchmark script is like spike-run-fs but adds tracing options by default.
   ./trace-benchmark --num-procs=5 --trace-destination=/tmp/test-trace tracer_tests/a02-near-atomic-friendly/a02-near-atomic-friendly.gcc.riscv64gc
```

# Trace format

Each trace is stored in a directory that contains a `trace.index` file and one or more `trace-XXXX.trc` files. The `trace.index` file consists of three lines:

 - The first line specifies the number of threads that are part of the trace.
 - The next two lines always contain, in the case of RISC-V traces, exactly the strings “TRACE_HAS_SEQUENCE_NUMBERS: 0” and “TRACE_HAS_SC_vs_RELAXED_LOCK_TYPE: 0,” and are used to identify the trace format version in gems4caps.

There are as many `trace-XXXX.trc` files as specified in the first line of `trace.index`. For each one, *XXXX* is the number of the thread whose instructions have been recorded in that file and varies from 0 to the number of threads minus 1 (adding zeros on the left to fill 4 digits). The numbers are assigned as threads start tracing. Optionally, `trace-XXXX.trc` files are compressed, and gems4caps and the tracer support uncompressed trace files or files compressed with lzma (default), zstd, or gzip. Ignoring the compression, each file lists the instructions executed by a thread in the following format:

 - The first line is the starting program counter of the trace, in hexadecimal.
 - Each line contains an instruction, encoded as follows:
   - The type of the instruction, identified by the sequence of letters until the first decimal digit. Note that the sequence may be empty (used to encode generic instructions).
   - The program counter of the instruction, encoded as the offset in decimal with respect to the previous instruction. Will be zero for the first instruction.
   - Operands of the instruction (including registers and memory locations, depending of the instruction type).
   - Branch information (if the instruction is a branch).

The included operands depend on the type of instruction. The format is as follows:

 - Registers that are read are listed preceded by the letter `y` (for data registers read by store instructions) or `x` (for everything else).
 - Registers that are written are listed preceded by the letter `z`.
 - Memory addresses that are read o written are formatted depending on the type of access:
   - Scalar accesses: the address in hexadecimal is printed preceded by a space, and then the size in decimal is printed separated by a space. 
   - Vector contiguous and strided accesses: the size in decimal is printed preceded by `s`, then the number of elements accessed in decimal preceded by `e`, then the address in hexadecimal of the first accessed element preceded by a space. If the stride is different than zero, it will be included in decimal after the first address, preceded by the character `+`.
   - Vector indexed accesses: the size in decimal is printed preceded by `s`, then the number of elements accessed in decimal preceded by `e`, then the list of addresses in hexadecimal accessed by the instruction, preceded by a space and separating each element with a comma (`,`).
   
 - The destination address for branches and jumps is listed as the offset in decimal with respect to the current instruction preceded by the letter `t`.. If the instruction is a taken branch, the character `*` will be added after the address.

Registers are encoded as integers in decimal. Values 0 to 31 correspond to RISC-V registers `x0` to `x31`, values 32 to 63 correspond to registers `f0` to `f31` and values 64 to 95 correspond to registers `v0` to `v31`. Note that scalar and vector instructions are differentiated only by the registers that they access.

The supported types of instructions and the operands that they include are:

 | Type                           | Prefix        | Operands                                                                |
 |--------------------------------|---------------|-------------------------------------------------------------------------|
 | Generic (e.g., ALU)            |               | x, z                                                                    |
 | Load                           | L             | x, z, memory                                                            |
 | Store                          | S             | x, y, memory                                                            |
 | Read-Modify-Write atomic       | RMW           | x, y, z, memory                                                         |
 | Load reserved                  | LR            | x, z, memory                                                            |
 | Store conditional              | SC            | x, y, z, memory                                                         |
 | Branch                         | B             | x, t                                                                    |
 | Direct call                    | C             | z, t                                                                    |
 | Indirect Call                  | c             | x, z, t                                                                 |
 | Direct Jump                    | J             | t                                                                       |
 | Indirect Jump                  | j             | x, t                                                                    |
 | Return                         | r             | x, t                                                                    |
 | Floating-point addition        | A             | x, z                                                                    |
 | Floating-point multiplication  | M             | x, z                                                                    |
 | Floating-point division        | D             | x, z                                                                    |
 | Floating-point square root     | Q             | x, z                                                                    |
 | Marker to start tracing        | START_TRACING |                                                                         |
 | Marker to start ROI            | CLEAR         |                                                                         |
 | Marker to stop tracing         | END_ROI       |                                                                         |
 | Mutex acquire                  | ACQ           | lock address (hex), thread id (dec)                                     |
 | Mutex release                  | REL           | lock address (hex), thread id (dec)                                     |
 | Barrier syncronization         | BAR           | conditional variable, counter and lock addresses (hex), thread id (dec) |
 | Conditional variable signal    | CV_SIGNAL     | conditional variable address (hex), thread id (dec)                     |
 | Conditional variable broadcast | CV_BCAST      | conditional variable address (hex), thread id (dec)                     |
 | Conditional variable wait      | CV_WAIT       | conditional variableand lock addresses (hex), thread id (dec)           |
 
Traces may include comments delimited by `{` and `}`. The tracer generates comments showing the original traced instructions if the `--log-g4trace-debug` is used.
  
Note that, although we the traces generated by the tracer follow the rules stated above, gems4caps accepts some variations in the format to support backward compatibility with previous versions. For example, instructions may be put in the same line and separated by spaces instead of newlines.

Below you can see an example excerpt of a generated trace, including comments. Note that `sltiu, zero, zero, 257` is the `START_TRACING` marker and `sltiu, zero, zero, 257` marks the beginning of the ROI.

    {    66666  sltiu   zero, zero, 257          } 1046e
    {    66670  lui     a5, 0x7e                 } 0z15
    {    66674  vsetvli a7, zero, e32, m1, ta, ma } 4x0z17
    {    66678  flw     fa5, -1896(a5)           } L4x15z47 7d898 4
    {    66682  lui     a1, 0x7e                 } 4z11
    {    66686  c.lui   a3, 0x18                 } 4z13
    {    66688  vid.v   v2                       } 2z66
    {    66692  addi    a0, a1, -1808            } 4x11z10
    {    66696  addi    a3, a3, 1696             } 4x13z13
    {    66700  addi    a1, a1, -1808            } 4x11z11
    {    66704  vfcvt.f.x.v v1, v2               } 4x66z65
    {    66708  vsetvli a4, a3, e8, mf4, ta, ma  } 4x13z14
    {    66712  vsetvli a5, zero, e32, m1, ta, ma } 4x0z15
    {    66716  vmv1r.v v3, v1                   } 4x65z67
    {    66720  vfmv.v.f v4, fa5                 } 4x64x47z68
    {    66724  vmv1r.v v5, v1                   } 4x65z69
    {    66728  vmv.v.x v1, a4                   } 4x64x14z65
    {    66732  vsetvli zero, a4, e32, m1, ta, ma } 4x14
    …
    {    66762  sw      zero, -2032(gp)          } S2x3y0 7d8a8 4
    {    66766  sltiu   zero, zero, 258          } CLEAR
    {    66770  lui     a5, 0x7e                 } 8z15
    {    66774  flw     fa4, -1892(a5)           } L4x15z46 7d89c 4
    {    66778  flw     fa5, -2032(gp)           } L4x3z47 7d8a8 4
    {    66782  vmv.v.i v1, 0                    } 4x64z65
    {    66786  vfmv.v.f v2, fa4                 } 4x64x46z66
    {    66790  c.lui   a3, 0x18                 } 4z13
    {    66792  c.mv    a2, a0                   } 2x10z12
    {    66794  addi    a3, a3, 1696             } 2x13z13
    {    66798  vsetvli a5, a3, e32, m1, tu, ma  } 4x13z15
    {    66802  vlseg3e32.v v3, (a2)             } L4x12z67z68z69s4e48 7d8f0 
    {    66806  vfadd.vv v1, v1, v2              } A4x65x66z65
    {    66810  slli    a4, a5, 1                } 4x15z14
    …
    {    66830  vfadd.vv v1, v1, v5              } A4x65x69z65
    {    66834  c.bnez  a3, pc - 36              } B4x13t-36*
    {    66798  vsetvli a5, a3, e32, m1, tu, ma  } -36x13z15
    {    66802  vlseg3e32.v v3, (a2)             } L4x12z67z68z69s4e48 7e130 
    {    66806  vfadd.vv v1, v1, v2              } A4x65x66z65
    {    66810  slli    a4, a5, 1                } 4x15z14
    {    66814  c.add   a4, a5                   } 4x14x15z14
    {    66816  c.slli  a4, 2                    } 2x14z14
    {    66818  c.sub   a3, a5                   } 2x13x15z13
    {    66820  c.add   a2, a4                   } 2x12x14z12
    {    66822  vfadd.vv v1, v1, v3              } A2x65x67z65
    {    66826  vfadd.vv v1, v1, v4              } A4x65x68z65
    {    66830  vfadd.vv v1, v1, v5              } A4x65x69z65
    {    66834  c.bnez  a3, pc - 36              } B4x13t-36*
    {    66798  vsetvli a5, a3, e32, m1, tu, ma  } -36x13z15
    {    66802  vlseg3e32.v v3, (a2)             } L4x12z67z68z69s4e48 7e1f0 
    …

## Known Bugs

 - ecall instructions are currently missing from the trace (and possibly other instructions that generate traps)

## TODO

 - add option `--log-use-roi-markers` (always enabled for now)
 - add option `--log-filter-privileged` (always enabled for now)

# Upstream README

The following sections are taken verbatim from the original README.md of Spike.


Spike RISC-V ISA Simulator
============================

About
-------------

Spike, the RISC-V ISA Simulator, implements a functional model of one or more
RISC-V harts.  It is named after the golden spike used to celebrate the
completion of the US transcontinental railway.

Spike supports the following RISC-V ISA features:
  - RV32I and RV64I base ISAs, v2.1
  - RV32E and RV64E base ISAs, v1.9
  - Zifencei extension, v2.0
  - Zicsr extension, v2.0
  - Zicntr extension, v2.0
  - M extension, v2.0
  - A extension, v2.1
  - B extension, v1.0
  - F extension, v2.2
  - D extension, v2.2
  - Q extension, v2.2
  - C extension, v2.0
  - Zbkb, Zbkc, Zbkx, Zknd, Zkne, Zknh, Zksed, Zksh scalar cryptography extensions (Zk, Zkn, and Zks groups), v1.0
  - Zkr virtual entropy source emulation, v1.0
  - V extension, v1.0 (_requires a 64-bit host_)
  - P extension, v0.9.2
  - Zba extension, v1.0
  - Zbb extension, v1.0
  - Zbc extension, v1.0
  - Zbs extension, v1.0
  - Zfh and Zfhmin half-precision floating-point extensions, v1.0
  - Zfinx extension, v1.0
  - Zmmul integer multiplication extension, v1.0
  - Zicbom, Zicbop, Zicboz cache-block maintenance extensions, v1.0
  - Conformance to both RVWMO and RVTSO (Spike is sequentially consistent)
  - Machine, Supervisor, and User modes, v1.11
  - Hypervisor extension, v1.0
  - Svnapot extension, v1.0
  - Svpbmt extension, v1.0
  - Svinval extension, v1.0
  - Svadu extension, v1.0
  - Svade extension, v1.0
  - Sdext extension, v1.0-STABLE
  - Sdtrig extension, v1.0-STABLE
  - Smepmp extension v1.0
  - Smstateen extension, v1.0
  - Smdbltrp extension, v1.0
  - Sscofpmf v0.5.2
  - Ssdbltrp extension, v1.0
  - Ssqosid extension, v1.0
  - Zaamo extension, v1.0
  - Zalrsc extension, v1.0
  - Zabha extension, v1.0
  - Zacas extension, v1.0
  - Zawrs extension, v1.0
  - Zicfiss extension, v1.0
  - Zicfilp extension, v1.0
  - Zca extension, v1.0
  - Zcb extension, v1.0
  - Zcf extension, v1.0
  - Zcd extension, v1.0
  - Zcmp extension, v1.0
  - Zcmt extension, v1.0
  - Zfbfmin extension, v0.6
  - Zvfbfmin extension, v0.6
  - Zvfbfwma extension, v0.6
  - Zvbb extension, v1.0
  - Zvbc extension, v1.0
  - Zvkg extension, v1.0
  - Zvkned extension, v1.0
  - Zvknha, Zvknhb extension, v1.0
  - Zvksed extension, v1.0
  - Zvksh extension, v1.0
  - Zvkt  extension, v1.0
  - Zvkn, Zvknc, Zvkng extension, v1.0
  - Zvks, Zvksc, Zvksg extension, v1.0 
  - Zicond extension, v1.0
  - Zilsd extension, v1.0
  - Zclsd extension, v1.0

Versioning and APIs
-------------------

Projects are versioned primarily to indicate when the API has been extended or
rendered incompatible.  In that spirit, Spike aims to follow the
[SemVer](https://semver.org/spec/v2.0.0.html) versioning scheme, in which
major version numbers are incremented when backwards-incompatible API changes
are made; minor version numbers are incremented when new APIs are added; and
patch version numbers are incremented when bugs are fixed in
a backwards-compatible manner.

Spike's principal public API is the RISC-V ISA.  _The C++ interface to Spike's
internals is **not** considered a public API at this time_, and
backwards-incompatible changes to this interface _will_ be made without
incrementing the major version number.

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools
install path.

    $ apt-get install device-tree-compiler libboost-regex-dev libboost-system-dev
    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV
    $ make
    $ [sudo] make install

If your system uses the `yum` package manager, you can substitute
`yum install dtc` for the first step.

Build Steps on OpenBSD
----------------------

Install bash, gmake, dtc, and use clang.

    $ pkg_add bash gmake dtc
    $ exec bash
    $ export CC=cc; export CXX=c++
    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV
    $ gmake
    $ [doas] make install

Compiling and Running a Simple C Program
-------------------------------------------

Install spike (see Build Steps), riscv-gnu-toolchain, and riscv-pk.

Write a short C program and name it hello.c.  Then, compile it into a RISC-V
ELF binary named hello:

    $ riscv64-unknown-elf-gcc -o hello hello.c

Now you can simulate the program atop the proxy kernel:

    $ spike pk hello

Simulating a New Instruction
------------------------------------

Adding an instruction to the simulator requires two steps:

  1.  Describe the instruction's functional behavior in the file
      riscv/insns/<new_instruction_name>.h.  Examine other instructions
      in that directory as a starting point.

  2.  Add the opcode and opcode mask to riscv/opcodes.h.  Alternatively,
      add it to the riscv-opcodes package, and it will do so for you:
        ```
         $ cd ../riscv-opcodes
         $ vi opcodes       // add a line for the new instruction
         $ make install
        ```

  3.  Add the instruction to riscv/riscv.mk.in. Otherwise, the instruction
      will not be included in the build and will be treated as an illegal instruction.

  4.  Rebuild the simulator.

Interactive Debug Mode
---------------------------

To invoke interactive debug mode, launch spike with -d:

    $ spike -d pk hello

To see the contents of an integer register (0 is for core 0):

    : reg 0 a0

To see the contents of a floating point register:

    : fregs 0 ft0

or:

    : fregd 0 ft0

depending upon whether you wish to print the register as single- or double-precision.

To see the contents of a memory location (physical address in hex):

    : mem 2020

To see the contents of memory with a virtual address (0 for core 0):

    : mem 0 2020

You can advance by one instruction by pressing the enter key. You can also
execute until a desired equality is reached:

    : until pc 0 2020                   (stop when pc=2020)
    : until reg 0 mie a                 (stop when register mie=0xa)
    : until mem 2020 50a9907311096993   (stop when mem[2020]=50a9907311096993)

Alternatively, you can execute as long as an equality is true:

    : while mem 2020 50a9907311096993

You can continue execution indefinitely by:

    : r

At any point during execution (even without -d), you can enter the
interactive debug mode with `<control>-<c>`.

To end the simulation from the debug prompt, press `<control>-<c>` or:

    : q

Debugging With Gdb
------------------

An alternative to interactive debug mode is to attach using gdb. Because spike
tries to be like real hardware, you also need OpenOCD to do that. OpenOCD
doesn't currently know about address translation, so it's not possible to
easily debug programs that are run under `pk`. We'll use the following test
program:
```
$ cat rot13.c 
char text[] = "Vafgehpgvba frgf jnag gb or serr!";

// Don't use the stack, because sp isn't set up.
volatile int wait = 1;

int main()
{
    while (wait)
        ;

    // Doesn't actually go on the stack, because there are lots of GPRs.
    int i = 0;
    while (text[i]) {
        char lower = text[i] | 32;
        if (lower >= 'a' && lower <= 'm')
            text[i] += 13;
        else if (lower > 'm' && lower <= 'z')
            text[i] -= 13;
        i++;
    }

done:
    while (!wait)
        ;
}
$ cat spike.lds 
OUTPUT_ARCH( "riscv" )

SECTIONS
{
  . = 0x10110000;
  .text : { *(.text) }
  .data : { *(.data) }
}
$ riscv64-unknown-elf-gcc -g -Og -o rot13-64.o -c rot13.c
$ riscv64-unknown-elf-gcc -g -Og -T spike.lds -nostartfiles -o rot13-64 rot13-64.o
```

To debug this program, first run spike telling it to listen for OpenOCD:
```
$ spike --rbb-port=9824 -m0x10100000:0x20000 rot13-64
Listening for remote bitbang connection on port 9824.
```

In a separate shell run OpenOCD with the appropriate configuration file:
```
$ cat spike.cfg 
adapter driver remote_bitbang
remote_bitbang host localhost
remote_bitbang port 9824

set _CHIPNAME riscv
jtag newtap $_CHIPNAME cpu -irlen 5 -expected-id 0xdeadbeef

set _TARGETNAME $_CHIPNAME.cpu
target create $_TARGETNAME riscv -chain-position $_TARGETNAME

gdb_report_data_abort enable

init
halt
$ openocd -f spike.cfg
Open On-Chip Debugger 0.10.0-dev-00002-gc3b344d (2017-06-08-12:14)
...
riscv.cpu: target state: halted
```

In yet another shell, start your gdb debug session:
```
tnewsome@compy-vm:~/SiFive/spike-test$ riscv64-unknown-elf-gdb rot13-64
GNU gdb (GDB) 8.0.50.20170724-git
Copyright (C) 2017 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "--host=x86_64-pc-linux-gnu --target=riscv64-unknown-elf".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from rot13-64...done.
(gdb) target remote localhost:3333
Remote debugging using localhost:3333
0x0000000010010004 in main () at rot13.c:8
8	    while (wait)
(gdb) print wait
$1 = 1
(gdb) print wait=0
$2 = 0
(gdb) print text
$3 = "Vafgehpgvba frgf jnag gb or serr!"
(gdb) b done 
Breakpoint 1 at 0x10110064: file rot13.c, line 22.
(gdb) c
Continuing.
Disabling abstract command writes to CSRs.

Breakpoint 1, main () at rot13.c:23
23	    while (!wait)
(gdb) print wait
$4 = 0
(gdb) print text
...
```
