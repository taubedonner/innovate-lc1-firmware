//Печатает все функции с адресами и именами.
//@category LC1
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class LC1Funcs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            println(String.format("%s\t%d\t%s",
                    f.getEntryPoint(), f.getBody().getNumAddresses(), f.getName()));
        }
    }
}
