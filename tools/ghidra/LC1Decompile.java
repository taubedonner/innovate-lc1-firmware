//Выгружает декомпиляцию всех функций программы в один файл.
//Аргумент: путь к выходному файлу.
//@category LC1
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class LC1Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String out = args.length > 0 ? args[0] : "/tmp/lc1-decompiled.c";

        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);

        PrintWriter w = new PrintWriter(out, "UTF-8");
        w.println("/* Декомпиляция " + currentProgram.getName()
                + ", язык " + currentProgram.getLanguageID() + " */");

        int ok = 0, fail = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled())
                break;
            w.println();
            w.println("/* ===== " + f.getName() + "  @" + f.getEntryPoint()
                    + "  " + f.getBody().getNumAddresses() + " байт ===== */");
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            if (r != null && r.decompileCompleted() && r.getDecompiledFunction() != null) {
                w.println(r.getDecompiledFunction().getC());
                ok++;
            } else {
                w.println("/* НЕ ДЕКОМПИЛИРОВАНА: "
                        + (r == null ? "нет результата" : r.getErrorMessage()) + " */");
                fail++;
            }
        }
        w.close();
        di.dispose();
        println("декомпилировано: " + ok + ", неудач: " + fail + " -> " + out);
    }
}
