//Печатает карту памяти, ссылки на диапазон mem:0x0100..0x0209 и сводку по функциям.
//@category LC1
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.*;

public class LC1Probe extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("### ПРОГРАММА: " + currentProgram.getName()
                + "  язык=" + currentProgram.getLanguageID());

        println("### БЛОКИ ПАМЯТИ");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println(String.format("  %-14s %s .. %s  %6d байт  r=%b w=%b x=%b init=%b",
                    b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                    b.isRead(), b.isWrite(), b.isExecute(), b.isInitialized()));
        }

        AddressSpace mem = currentProgram.getAddressFactory().getAddressSpace("mem");
        if (mem == null) { println("!! нет адресного пространства mem"); return; }

        println("### ССЫЛКИ НА mem:0x0100..0x0209");
        ReferenceManager rm = currentProgram.getReferenceManager();
        int total = 0;
        for (long a = 0x0100; a <= 0x0209; a++) {
            Address t = mem.getAddress(a);
            ReferenceIterator it = rm.getReferencesTo(t);
            while (it.hasNext()) {
                Reference r = it.next();
                Address from = r.getFromAddress();
                Function f = getFunctionContaining(from);
                println(String.format("  mem:%04x  <- %s  (%s)  тип=%s",
                        a, from, f == null ? "-" : f.getName(), r.getReferenceType()));
                total++;
            }
        }
        println("  всего ссылок: " + total);

        println("### СИМВОЛЫ В mem:0x0100..0x0209");
        SymbolTable st = currentProgram.getSymbolTable();
        for (long a = 0x0100; a <= 0x0209; a++) {
            for (Symbol s : st.getSymbols(mem.getAddress(a)))
                println(String.format("  mem:%04x  %s  (%s)", a, s.getName(), s.getSymbolType()));
        }

        int nf = 0, named = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            nf++;
            if (!f.getName().startsWith("FUN_") && !f.getName().startsWith("LAB_")) named++;
        }
        println("### ФУНКЦИЙ: " + nf + ", из них с осмысленными именами: " + named);
    }
}
