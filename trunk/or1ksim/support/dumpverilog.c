/* dumpverilog.c -- Dumps memory region as Verilog representation
   or as hex code

   Copyright (C) 2000 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2008 Embecosm Limited

   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of Or1ksim, the OpenRISC 1000 Architectural Simulator.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This program is commented throughout in a fashion suitable for processing
   with Doxygen. */

/* Verilog dump can be used for stimulating OpenRISC Verilog RTL models. */


/* Autoconf and/or portability configuration */
#include "config.h"

/* Package includes */
#include "sim-config.h"
#include "arch.h"
#include "abstract.h"
#include "labels.h"
#include "opcode/or32.h"


#define DW 32			/* Data width of mem model generated by */
				/* dumpverilog in bits */
#define DWQ (DW/8)		/* Same as DW but units are bytes */
#define DISWIDTH 25		/* Width of disassembled message in bytes */

#define OR1K_MEM_VERILOG_HEADER(MODNAME, FROMADDR, TOADDR, DISWIDTH) "\n"\
"include \"general.h\"\n\n"\
"`timescale 1ns/100ps\n\n"\
"// Simple dw-wide Sync SRAM with initial content generated by or1ksim.\n"\
"// All control, data in and addr signals are sampled at rising clock edge  \n"\
"// Data out is not registered. Address bits specify dw-word (narrowest \n"\
"// addressed data is not byte but dw-word !). \n"\
"// There are still some bugs in generated output (dump word aligned regions)\n\n"\
"module %s(clk, data, addr, ce, we, disout);\n\n"\
"parameter dw = 32;\n"\
"parameter amin = %" PRIdREG ";\n\n"\
"parameter amax = %" PRIdREG ";\n\n"\
"input clk;\n"\
"inout [dw-1:0] data;\n"\
"input [31:0] addr;\n"\
"input ce;\n"\
"input we;\n"\
"output [%d:0] disout;\n\n"\
"reg  [%d:0] disout;\n"\
"reg  [dw-1:0] mem [amax:amin];\n"\
"reg  [%d:0] dis [amax:amin];\n"\
"reg  [dw-1:0] dataout;\n"\
"tri  [dw-1:0] data = (ce && ~we) ? dataout : 'bz;\n\n"\
"initial begin\n", MODNAME, FROMADDR, TOADDR, DISWIDTH-1, DISWIDTH-1, DISWIDTH-1

#define OR1K_MEM_VERILOG_FOOTER "\n\
end\n\n\
always @(posedge clk) begin\n\
        if (ce && ~we) begin\n\
                dataout <= #1 mem[addr];\n\
                disout <= #1 dis[addr];\n\
                $display(\"or1k_mem: reading mem[%%0d]:%%h dis: %%0s\", addr, dataout, dis[addr]);\n\
        end else\n\
        if (ce && we) begin\n\
                mem[addr] <= #1 data;\n\
                dis[addr] <= #1 \"(data)\";\n\
                $display(\"or1k_mem: writing mem[%%0d]:%%h dis: %%0s\", addr, mem[addr], dis[addr]);\n\
        end\n\
end\n\n\
endmodule\n"

#define LABELEND_CHAR	":"

void
dumpverilog (char *verilog_modname, oraddr_t from, oraddr_t to)
{
  unsigned int i, done = 0;
  struct label_entry *tmp;
  char dis[DISWIDTH + 100];
  uint32_t insn;
  int index;
  PRINTF ("// This file was generated by or1ksim version %s\n",
	  PACKAGE_VERSION);
  PRINTF (OR1K_MEM_VERILOG_HEADER
	  (verilog_modname, from / DWQ, to / DWQ, (DISWIDTH * 8)));

  for (i = from; i < to; i++)
    {
      if (!(i & 3))
	{
	  insn = eval_direct32 (i, 0, 0);
	  index = or1ksim_insn_decode (insn);
	  if (index >= 0)
	    {
	      if (verify_memoryarea (i) && (tmp = get_label (i)))
		if (tmp)
		  PRINTF ("\n//\t%s%s", tmp->name, LABELEND_CHAR);

	      PRINTF ("\n\tmem['h%x] = %d'h%.8" PRIx32 ";", i / DWQ, DW,
		      eval_direct32 (i, 0, 0));

	      or1ksim_disassemble_insn (insn);
	      strcpy (dis, or1ksim_disassembled);

	      if (strlen (dis) < DISWIDTH)
		memset (dis + strlen (dis), ' ', DISWIDTH);
	      dis[DISWIDTH] = '\0';
	      PRINTF ("\n\tdis['h%x] = {\"%s\"};", i / DWQ, dis);
	      dis[0] = '\0';
	      i += or1ksim_insn_len (index) - 1;
	      done = 1;
	      continue;
	    }
	}

      if (i % 64 == 0)
	PRINTF ("\n");

      PRINTF ("\n\tmem['h%x] = 'h%.2x;", i / DWQ, eval_direct8 (i, 0, 0));
      done = 1;
    }

  if (done)
    {
      PRINTF (OR1K_MEM_VERILOG_FOOTER);
      return;
    }

  /* this needs to be fixed */

  for (i = from; i < to; i++)
    {
      if (i % 8 == 0)
	PRINTF ("\n%.8x:  ", i);

      /* don't print ascii chars below 0x20. */
      if (eval_direct32 (i, 0, 0) < 0x20)
	PRINTF ("0x%.2x     ", (uint8_t) eval_direct32 (i, 0, 0));
      else
	PRINTF ("0x%.2x'%c'  ", (uint8_t) eval_direct32 (i, 0, 0),
		(char) eval_direct32 (i, 0, 0));
    }
  PRINTF (OR1K_MEM_VERILOG_FOOTER);
}

void
dumphex (oraddr_t from, oraddr_t to)
{
  oraddr_t i;
  uint32_t insn;
  int index;

  for (i = from; i < to; i++)
    {
      if (!(i & 3))
	{
	  insn = eval_direct32 (i, 0, 0);
	  index = or1ksim_insn_decode (insn);
	  if (index >= 0)
	    {
	      PRINTF ("%.8" PRIx32 "\n", eval_direct32 (i, 0, 0));
	      i += or1ksim_insn_len (index) - 1;
	      continue;
	    }
	}
      PRINTF ("%.2x\n", eval_direct8 (i, 0, 0));
    }
}
