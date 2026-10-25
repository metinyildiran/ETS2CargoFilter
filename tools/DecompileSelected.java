// Research-only, source-immutable selective Ghidra decompilation.
// Run on a temporary imported project with -noanalysis; this is NOT complete
// analysis and inferred signatures/types must not be treated as a verified ABI.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.nio.file.Files;
import java.nio.file.Path;

public class DecompileSelected extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output-directory address...");
        Path destination = Path.of(args[0]);
        Files.createDirectories(destination);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for (int i = 1; i < args.length; i++) {
                var address = toAddr(args[i]);
                disassemble(address);
                Function function = getFunctionAt(address);
                if (function == null) function = createFunction(address, null);
                if (function == null) throw new IllegalStateException("No function at " + address);
                var result = decompiler.decompileFunction(function, 30, monitor);
                String text = result.decompileCompleted()
                    ? result.getDecompiledFunction().getC()
                    : "DECOMPILATION INCOMPLETE: " + result.getErrorMessage();
                Files.writeString(destination.resolve(address + ".c.txt"), text);
                println("SELECTED_FUNCTION " + address + " complete=" + result.decompileCompleted());
            }
        } finally {
            decompiler.dispose();
        }
    }
}
